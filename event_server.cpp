/*
 *  event_server.cpp
 *  PHD Guiding
 *
 *  Created by Andy Galasso.
 *  Copyright (c) 2013 Andy Galasso.
 *  All rights reserved.
 *
 *  This source code is distributed under the following "BSD" license
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *    Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *    Neither the name of Craig Stark, Stark Labs nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "phd.h"

#include <wx/sstream.h>
#include <wx/sckstrm.h>
#include <sstream>

EventServer EvtServer;

BEGIN_EVENT_TABLE(EventServer, wxEvtHandler)
    EVT_SOCKET(EVENT_SERVER_ID, EventServer::OnEventServerEvent)
    EVT_SOCKET(EVENT_SERVER_CLIENT_ID, EventServer::OnEventServerClientEvent)
END_EVENT_TABLE()

enum
{
    MSG_PROTOCOL_VERSION = 1,
};

static const wxString literal_null("null");
static const wxString literal_true("true");
static const wxString literal_false("false");

static wxString state_name(EXPOSED_STATE st)
{
    switch (st)
    {
        case EXPOSED_STATE_NONE:             return "Stopped";
        case EXPOSED_STATE_SELECTED:         return "Selected";
        case EXPOSED_STATE_CALIBRATING:      return "Calibrating";
        case EXPOSED_STATE_GUIDING_LOCKED:   return "Guiding";
        case EXPOSED_STATE_GUIDING_LOST:     return "LostLock";
        case EXPOSED_STATE_PAUSED:           return "Paused";
        case EXPOSED_STATE_LOOPING:          return "Looping";
        default:                             return "Unknown";
    }
}

static wxString json_escape(const wxString& s)
{
    wxString t(s);
    static const wxString BACKSLASH("\\");
    static const wxString BACKSLASHBACKSLASH("\\\\");
    static const wxString DQUOT("\"");
    static const wxString BACKSLASHDQUOT("\\\"");
    t.Replace(BACKSLASH, BACKSLASHBACKSLASH);
    t.Replace(DQUOT, BACKSLASHDQUOT);
    return t;
}

template<char LDELIM, char RDELIM>
struct JSeq
{
    wxString m_s;
    bool m_first;
    bool m_closed;
    JSeq() : m_first(true), m_closed(false) { m_s << LDELIM; }
    void close() { m_s << RDELIM; m_closed = true; }
    wxString str() { if (!m_closed) close(); return m_s; }
};

typedef JSeq<'[', ']'> JAry;
typedef JSeq<'{', '}'> JObj;

static JAry& operator<<(JAry& a, const wxString& str)
{
    if (a.m_first)
        a.m_first = false;
    else
        a.m_s << ',';
    a.m_s << str;
    return a;
}

static JAry& operator<<(JAry& a, double d)
{
    return a << wxString::Format("%.2f", d);
}

static JAry& operator<<(JAry& a, int i)
{
    return a << wxString::Format("%d", i);
}

static wxString json_format(const json_value *j)
{
    if (!j)
        return literal_null;

    switch (j->type) {
    default:
    case JSON_NULL: return literal_null;
    case JSON_OBJECT: {
        wxString ret("{");
        bool first = true;
        json_for_each (jj, j)
        {
            if (first)
                first = false;
            else
                ret << ",";
            ret << '"' << jj->name << "\":" << json_format(jj);
        }
        ret << "}";
        return ret;
    }
    case JSON_ARRAY: {
        wxString ret("[");
        bool first = true;
        json_for_each (jj, j)
        {
            if (first)
                first = false;
            else
                ret << ",";
            ret << json_format(jj);
        }
        ret << "]";
        return ret;
    }
    case JSON_STRING: return '"' + json_escape(j->string_value) + '"';
    case JSON_INT:    return wxString::Format("%d", j->int_value);
    case JSON_FLOAT:  return wxString::Format("%g", (double) j->float_value);
    case JSON_BOOL:   return j->int_value ? literal_true : literal_false;
    }
}

struct NULL_TYPE { } NULL_VALUE;

// name-value pair
struct NV
{
    wxString n;
    wxString v;
    NV(const wxString& n_, const wxString& v_) : n(n_), v('"' + json_escape(v_) + '"') { }
    NV(const wxString& n_, const char *v_) : n(n_), v('"' + json_escape(v_) + '"') { }
    NV(const wxString& n_, const wchar_t *v_) : n(n_), v('"' + json_escape(v_) + '"') { }
    NV(const wxString& n_, int v_) : n(n_), v(wxString::Format("%d", v_)) { }
    NV(const wxString& n_, double v_) : n(n_), v(wxString::Format("%g", v_)) { }
    NV(const wxString& n_, double v_, int prec) : n(n_), v(wxString::Format("%.*f", prec, v_)) { }
    NV(const wxString& n_, bool v_) : n(n_), v(v_ ? literal_true : literal_false) { }
    template<typename T>
    NV(const wxString& n_, const std::vector<T>& vec);
    NV(const wxString& n_, JAry& ary) : n(n_), v(ary.str()) { }
    NV(const wxString& n_, JObj& obj) : n(n_), v(obj.str()) { }
    NV(const wxString& n_, const json_value *v_) : n(n_), v(json_format(v_)) { }
    NV(const wxString& n_, const PHD_Point& p) : n(n_) { JAry ary; ary << p.X << p.Y; v = ary.str(); }
    NV(const wxString& n_, const wxPoint& p) : n(n_) { JAry ary; ary << p.x << p.y; v = ary.str(); }
    NV(const wxString& n_, const NULL_TYPE& nul) : n(n_), v(literal_null) { }
};

template<typename T>
NV::NV(const wxString& n_, const std::vector<T>& vec)
    : n(n_)
{
    std::ostringstream os;
    os << '[';
    for (unsigned int i = 0; i < vec.size(); i++)
    {
        if (i != 0)
            os << ',';
        os << vec[i];
    }
    os << ']';
    v = os.str();
}

static JObj& operator<<(JObj& j, const NV& nv)
{
    if (j.m_first)
        j.m_first = false;
    else
        j.m_s << ',';
    j.m_s << '"' << nv.n << "\":" << nv.v;
    return j;
}

static NV NVMount(const Mount *mount)
{
    return NV("Mount", mount->Name());
}

static JObj& operator<<(JObj& j, const PHD_Point& pt)
{
    return j << NV("X", pt.X, 3) << NV("Y", pt.Y, 3);
}

static JAry& operator<<(JAry& a, JObj& j)
{
    return a << j.str();
}

struct Ev : public JObj
{
    Ev(const wxString& event)
    {
        double const now = ::wxGetUTCTimeMillis().ToDouble() / 1000.0;
        *this << NV("Event", event)
            << NV("Timestamp", now, 3)
            << NV("Host", wxGetHostName())
            << NV("Inst", pFrame->GetInstanceNumber());
    }
};

static Ev ev_message_version()
{
    Ev ev("Version");
    ev << NV("PHDVersion", PHDVERSION)
        << NV("PHDSubver", PHDSUBVER)
        << NV("MsgVersion", MSG_PROTOCOL_VERSION);
    return ev;
}

static Ev ev_set_lock_position(const PHD_Point& xy)
{
    Ev ev("LockPositionSet");
    ev << xy;
    return ev;
}

static Ev ev_calibration_complete(Mount *mount)
{
    Ev ev("CalibrationComplete");
    ev << NVMount(mount);

    if (mount->IsStepGuider())
    {
        ev << NV("Limit", mount->GetAoMaxPos());
    }

    return ev;
}

static Ev ev_star_selected(const PHD_Point& pos)
{
    Ev ev("StarSelected");
    ev << pos;
    return ev;
}

static Ev ev_start_guiding()
{
    return Ev("StartGuiding");
}

static Ev ev_paused()
{
    return Ev("Paused");
}

static Ev ev_start_calibration(Mount *mount)
{
    Ev ev("StartCalibration");
    ev << NVMount(mount);
    return ev;
}

static Ev ev_app_state(EXPOSED_STATE st = Guider::GetExposedState())
{
    Ev ev("AppState");
    ev << NV("State", state_name(st));
    return ev;
}

static Ev ev_settling(double distance, double time, double settleTime)
{
    Ev ev("Settling");

    ev << NV("Distance", distance, 2)
       << NV("Time", time, 1)
       << NV("SettleTime", settleTime, 1);

    return ev;
}

static Ev ev_settle_done(const wxString& errorMsg)
{
    Ev ev("SettleDone");

    int status = errorMsg.IsEmpty() ? 0 : 1;

    ev << NV("Status", status);

    if (status != 0)
    {
        ev << NV("Error", errorMsg);
    }

    return ev;
}

struct ClientReadBuf
{
    enum { SIZE = 1024 };
    char buf[SIZE];
    char *dest;

    ClientReadBuf() { reset(); }
    size_t avail() const { return &buf[SIZE] - dest; }
    void reset() { dest = &buf[0]; }
};

struct ClientData
{
    wxSocketClient *cli;
    int refcnt;
    ClientReadBuf rdbuf;
    wxMutex wrlock;

    ClientData(wxSocketClient *cli_) : cli(cli_), refcnt(1) { }
    void AddRef() { ++refcnt; }
    void RemoveRef()
    {
        if (--refcnt == 0)
        {
            cli->Destroy();
            delete this;
        }
    }
};

struct ClientDataGuard
{
    ClientData *cd;
    ClientDataGuard(wxSocketClient *cli) : cd((ClientData *) cli->GetClientData()) { cd->AddRef(); }
    ~ClientDataGuard() { cd->RemoveRef(); }
    ClientData *operator->() const { return cd; }
};

inline static wxMutex *client_wrlock(wxSocketClient *cli)
{
    return &((ClientData *) cli->GetClientData())->wrlock;
}

static void send_buf(wxSocketClient *client, const wxCharBuffer& buf)
{
    wxMutexLocker lock(*client_wrlock(client));
    client->Write(buf.data(), buf.length());
    if (client->LastWriteCount() != buf.length())
    {
        Debug.Write(wxString::Format("evsrv: cli %p short write %u/%u\n",
            client, client->LastWriteCount(), (unsigned int) buf.length()));
    }
}

static void do_notify1(wxSocketClient *client, const JAry& ary)
{
    send_buf(client, (JAry(ary).str() + "\r\n").ToUTF8());
}

static void do_notify1(wxSocketClient *client, const JObj& j)
{
    send_buf(client, (JObj(j).str() + "\r\n").ToUTF8());
}

static void do_notify(const EventServer::CliSockSet& cli, const JObj& jj)
{
    wxCharBuffer buf = (JObj(jj).str() + "\r\n").ToUTF8();

    for (EventServer::CliSockSet::const_iterator it = cli.begin();
        it != cli.end(); ++it)
    {
        send_buf(*it, buf);
    }
}

inline static void simple_notify(const EventServer::CliSockSet& cli, const wxString& ev)
{
    if (!cli.empty())
        do_notify(cli, Ev(ev));
}

inline static void simple_notify_ev(const EventServer::CliSockSet& cli, const Ev& ev)
{
    if (!cli.empty())
        do_notify(cli, ev);
}

#define SIMPLE_NOTIFY(s) simple_notify(m_eventServerClients, s)
#define SIMPLE_NOTIFY_EV(ev) simple_notify_ev(m_eventServerClients, ev)

static void send_catchup_events(wxSocketClient *cli)
{
    EXPOSED_STATE st = Guider::GetExposedState();

    do_notify1(cli, ev_message_version());

    if (pFrame->pGuider)
    {
        if (pFrame->pGuider->LockPosition().IsValid())
            do_notify1(cli, ev_set_lock_position(pFrame->pGuider->LockPosition()));

        if (pFrame->pGuider->CurrentPosition().IsValid())
            do_notify1(cli, ev_star_selected(pFrame->pGuider->CurrentPosition()));
    }

    if (pMount && pMount->IsCalibrated())
        do_notify1(cli, ev_calibration_complete(pMount));

    if (pSecondaryMount && pSecondaryMount->IsCalibrated())
        do_notify1(cli, ev_calibration_complete(pSecondaryMount));

    if (st == EXPOSED_STATE_GUIDING_LOCKED)
    {
        do_notify1(cli, ev_start_guiding());
    }
    else if (st == EXPOSED_STATE_CALIBRATING)
    {
        Mount *mount = pMount;
        if (pFrame->pGuider->GetState() == STATE_CALIBRATING_SECONDARY)
            mount = pSecondaryMount;
        do_notify1(cli, ev_start_calibration(mount));
    }
    else if (st == EXPOSED_STATE_PAUSED) {
        do_notify1(cli, ev_paused());
    }

    do_notify1(cli, ev_app_state());
}

static void destroy_client(wxSocketClient *cli)
{
    ClientData *buf = (ClientData *) cli->GetClientData();
    buf->RemoveRef();
}

static void drain_input(wxSocketInputStream& sis)
{
    while (sis.CanRead())
    {
        char buf[1024];
        if (sis.Read(buf, sizeof(buf)).LastRead() == 0)
            break;
    }
}

static bool find_eol(char *p, size_t len)
{
    const char *end = p + len;
    for (; p < end; p++)
    {
        if (*p == '\r' || *p == '\n')
        {
            *p = 0;
            return true;
        }
    }
    return false;
}

enum {
    JSONRPC_PARSE_ERROR = -32700,
    JSONRPC_INVALID_REQUEST = -32600,
    JSONRPC_METHOD_NOT_FOUND = -32601,
    JSONRPC_INVALID_PARAMS = -32602,
    JSONRPC_INTERNAL_ERROR = -32603,
};

static NV jrpc_error(int code, const wxString& msg)
{
    JObj err;
    err << NV("code", code) << NV("message", msg);
    return NV("error", err);
}

template<typename T>
static NV jrpc_result(const T& t)
{
    return NV("result", t);
}

template<typename T>
static NV jrpc_result(T& t)
{
    return NV("result", t);
}

static NV jrpc_id(const json_value *id)
{
    return NV("id", id);
}

struct JRpcResponse : public JObj
{
    JRpcResponse() { *this << NV("jsonrpc", "2.0"); }
};

static wxString parser_error(const JsonParser& parser)
{
    return wxString::Format("invalid JSON request: %s on line %d at \"%.12s...\"",
        parser.ErrorDesc(), parser.ErrorLine(), parser.ErrorPos());
}

static void
parse_request(const json_value *req, const json_value **pmethod, const json_value **pparams,
              const json_value **pid)
{
    *pmethod = *pparams = *pid = 0;

    if (req)
    {
        json_for_each (t, req)
        {
            if (t->name)
            {
                if (t->type == JSON_STRING && strcmp(t->name, "method") == 0)
                    *pmethod = t;
                else if (strcmp(t->name, "params") == 0)
                    *pparams = t;
                else if (strcmp(t->name, "id") == 0)
                    *pid = t;
            }
        }
    }
}

static const json_value *at(const json_value *ary, unsigned int idx)
{
    unsigned int i = 0;
    json_for_each (j, ary)
    {
        if (i == idx)
            return j;
        ++i;
    }
    return 0;
}

// paranoia
#define VERIFY_GUIDER(response) do { \
    if (!pFrame || !pFrame->pGuider) \
    { \
        response << jrpc_error(1, "internal error"); \
        return; \
    } \
} while (0)

static void deselect_star(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);
    pFrame->pGuider->Reset(true);
    response << jrpc_result(0);
}

static void get_exposure(JObj& response, const json_value *params)
{
    response << jrpc_result(pFrame->RequestedExposureDuration());
}

static void get_exposure_durations(JObj& response, const json_value *params)
{
    std::vector<int> exposure_durations;
    pFrame->GetExposureDurations(&exposure_durations);
    response << jrpc_result(exposure_durations);
}

static void get_profiles(JObj& response, const json_value *params)
{
    JAry ary;
    wxArrayString names = pConfig->ProfileNames();
    for (unsigned int i = 0; i < names.size(); i++)
    {
        wxString name = names[i];
        int id = pConfig->GetProfileId(name);
        if (id)
        {
            JObj t;
            t << NV("id", id) << NV("name", name);
            if (id == pConfig->GetCurrentProfileId())
                t << NV("selected", true);
            ary << t;
        }
    }
    response << jrpc_result(ary);
}

struct Params
{
    std::map<std::string, const json_value *> dict;

    void Init(const char *names[], size_t nr_names, const json_value *params)
    {
        if (!params)
            return;
        if (params->type == JSON_ARRAY)
        {
            const json_value *jv = params->first_child;
            for (size_t i = 0; jv && i < nr_names; i++, jv = jv->next_sibling)
            {
                const char *name = names[i];
                dict.insert(std::make_pair(std::string(name), jv));
            }
        }
        else if (params->type == JSON_OBJECT)
        {
            json_for_each(jv, params)
            {
                dict.insert(std::make_pair(std::string(jv->name), jv));
            }
        }
    }
    Params(const char *n1, const json_value *params)
    {
        const char *n[] = { n1 };
        Init(n, 1, params);
    }
    Params(const char *n1, const char *n2, const json_value *params)
    {
        const char *n[] = { n1, n2 };
        Init(n, 2, params);
    }
    Params(const char *n1, const char *n2, const char *n3, const json_value *params)
    {
        const char *n[] = { n1, n2, n3 };
        Init(n, 3, params);
    }
    Params(const char *n1, const char *n2, const char *n3, const char *n4, const json_value *params)
    {
        const char *n[] = { n1, n2, n3, n4 };
        Init(n, 4, params);
    }
    const json_value *param(const std::string& name) const
    {
        auto it = dict.find(name);
        return it == dict.end() ? 0 : it->second;
    }
};

static void set_exposure(JObj& response, const json_value *params)
{
    Params p("exposure", params);
    const json_value *exp = p.param("exposure");

    if (!exp || exp->type != JSON_INT)
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected exposure param");
        return;
    }

    bool ok = pFrame->SetExposureDuration(exp->int_value);
    if (ok)
    {
        response << jrpc_result(1);
    }
    else
    {
        response << jrpc_error(1, "could not set exposure duration");
    }
}

static void get_profile(JObj& response, const json_value *params)
{
    int id = pConfig->GetCurrentProfileId();
    wxString name = pConfig->GetCurrentProfile();
    JObj t;
    t << NV("id", id) << NV("name", name);
    response << jrpc_result(t);
}

static bool all_equipment_connected()
{
    return pCamera && pCamera->Connected &&
        (!pMount || pMount->IsConnected()) &&
        (!pSecondaryMount || pSecondaryMount->IsConnected());
}

static void set_profile(JObj& response, const json_value *params)
{
    Params p("id", params);
    const json_value *id = p.param("id");
    if (!id || id->type != JSON_INT)
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected profile id param");
        return;
    }

    VERIFY_GUIDER(response);

    wxString errMsg;
    bool error = pFrame->pGearDialog->SetProfile(id->int_value, &errMsg);

    if (error)
    {
        response << jrpc_error(1, errMsg);
    }
    else
    {
        response << jrpc_result(0);
    }
}

static void get_connected(JObj& response, const json_value *params)
{
    response << jrpc_result(all_equipment_connected());
}

static void set_connected(JObj& response, const json_value *params)
{
    Params p("connected", params);
    const json_value *val = p.param("connected");
    if (!val || val->type != JSON_BOOL)
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected connected boolean param");
        return;
    }

    VERIFY_GUIDER(response);

    wxString errMsg;
    bool error = val->int_value ? pFrame->pGearDialog->ConnectAll(&errMsg) :
        pFrame->pGearDialog->DisconnectAll(&errMsg);

    if (error)
    {
        response << jrpc_error(1, errMsg);
    }
    else
    {
        response << jrpc_result(0);
    }
}

static void get_calibrated(JObj& response, const json_value *params)
{
    bool calibrated = pMount && pMount->IsCalibrated() && (!pSecondaryMount || pSecondaryMount->IsCalibrated());
    response << jrpc_result(calibrated);
}

static bool float_param(const json_value *v, double *p)
{
    if (v->type == JSON_INT)
    {
        *p = (double) v->int_value;
        return true;
    }
    else if (v->type == JSON_FLOAT)
    {
        *p = v->float_value;
        return true;
    }

    return false;
}

static bool float_param(const char *name, const json_value *v, double *p)
{
    if (strcmp(name, v->name) != 0)
        return false;

    return float_param(v, p);
}

inline static bool bool_value(const json_value *v)
{
    return v->int_value ? true : false;
}

static bool bool_param(const json_value *jv, bool *val)
{
    if (jv->type != JSON_BOOL && jv->type != JSON_INT)
        return false;
    *val = bool_value(jv);
    return true;
}

static void get_paused(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);
    response << jrpc_result(pFrame->pGuider->IsPaused());
}

static void set_paused(JObj& response, const json_value *params)
{
    Params p("paused", "type", params);
    const json_value *jv = p.param("paused");

    bool val;
    if (!jv || !bool_param(jv, &val))
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected bool param at index 0");
        return;
    }

    PauseType pause = PAUSE_NONE;

    if (val)
    {
        pause = PAUSE_GUIDING;

        jv = p.param("type");
        if (jv)
        {
            if (jv->type == JSON_STRING)
            {
                if (strcmp(jv->string_value, "full") == 0)
                    pause = PAUSE_FULL;
            }
            else
            {
                response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected string param at index 1");
                return;
            }
        }
    }

    pFrame->SetPaused(pause);

    response << jrpc_result(0);
}

static void loop(JObj& response, const json_value *params)
{
    bool error = pFrame->StartLooping();

    if (error)
        response << jrpc_error(1, "could not start looping");
    else
        response << jrpc_result(0);
}

static void stop_capture(JObj& response, const json_value *params)
{
    pFrame->StopCapturing();
    response << jrpc_result(0);
}

static void find_star(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);

    bool error = pFrame->pGuider->AutoSelect();

    if (!error)
    {
        const PHD_Point& lockPos = pFrame->pGuider->LockPosition();
        if (lockPos.IsValid())
        {
            response << jrpc_result(lockPos);
            return;
        }
    }

    response << jrpc_error(1, "could not find star");
}

static void get_pixel_scale(JObj& response, const json_value *params)
{
    double scale = pFrame->GetCameraPixelScale();
    if (scale == 1.0)
        response << jrpc_result(NULL_VALUE); // scale unknown
    else
        response << jrpc_result(scale);
}

static void get_app_state(JObj& response, const json_value *params)
{
    EXPOSED_STATE st = Guider::GetExposedState();
    response << jrpc_result(state_name(st));
}

static void get_lock_position(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);

    const PHD_Point& lockPos = pFrame->pGuider->LockPosition();
    if (lockPos.IsValid())
        response << jrpc_result(lockPos);
    else
        response << jrpc_result(NULL_VALUE);
}

// {"method": "set_lock_position", "params": [X, Y, true], "id": 1}
static void set_lock_position(JObj& response, const json_value *params)
{
    Params p("x", "y", "exact", params);
    const json_value *p0 = p.param("x"), *p1 = p.param("y");
    double x, y;

    if (!p0 || !p1 || !float_param(p0, &x) || !float_param(p1, &y))
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected lock position x, y params");
        return;
    }

    bool exact = true;
    const json_value *p2 = p.param("exact");

    if (p2)
    {
        if (!bool_param(p2, &exact))
        {
            response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected boolean param at index 2");
            return;
        }
    }

    VERIFY_GUIDER(response);

    bool error;

    if (exact)
        error = pFrame->pGuider->SetLockPosition(PHD_Point(x, y));
    else
        error = pFrame->pGuider->SetLockPosToStarAtPosition(PHD_Point(x, y));

    if (error)
    {
        response << jrpc_error(JSONRPC_INVALID_REQUEST, "could not set lock position");
        return;
    }

    response << jrpc_result(0);
}

inline static const char *string_val(const json_value *j)
{
    return j->type == JSON_STRING ? j->string_value : "";
}

static void clear_calibration(JObj& response, const json_value *params)
{
    bool clear_mount;
    bool clear_ao;

    if (!params)
    {
        clear_mount = clear_ao = true;
    }
    else
    {
        clear_mount = clear_ao = false;

        json_for_each (val, params)
        {
            const char *which = string_val(val);
            if (strcmp(which, "mount") == 0)
                clear_mount = true;
            else if (strcmp(which, "ao") == 0)
                clear_ao = true;
            else if (strcmp(which, "both") == 0)
                clear_mount = clear_ao = true;
            else
            {
                response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected param \"mount\", \"ao\", or \"both\"");
                return;
            }
        }
    }

    Mount *mount;
    Mount *ao;
    if (pMount && pMount->IsStepGuider())
        ao = pMount, mount = pSecondaryMount;
    else
        ao = 0, mount = pMount;

    if (mount && clear_mount)
        mount->ClearCalibration();

    if (ao && clear_ao)
        ao->ClearCalibration();

    response << jrpc_result(0);
}

static void flip_calibration(JObj& response, const json_value *params)
{
    bool error = pFrame->FlipRACal();

    if (error)
        response << jrpc_error(1, "could not flip calibration");
    else
        response << jrpc_result(0);
}

static void get_lock_shift_enabled(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);
    bool enabled = pFrame->pGuider->GetLockPosShiftParams().shiftEnabled;
    response << jrpc_result(enabled);
}

static void set_lock_shift_enabled(JObj& response, const json_value *params)
{
    Params p("enabled", params);
    const json_value *val = p.param("enabled");
    bool enable;
    if (!val || !bool_param(val, &enable))
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected enabled boolean param");
        return;
    }

    VERIFY_GUIDER(response);

    pFrame->pGuider->EnableLockPosShift(enable);

    response << jrpc_result(0);
}

static void get_lock_shift_params(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);
    const LockPosShiftParams& lockShift = pFrame->pGuider->GetLockPosShiftParams();
    JObj rslt;
    rslt << NV("enabled", lockShift.shiftEnabled);
    if (lockShift.shiftRate.IsValid())
    {
        rslt << NV("rate", lockShift.shiftRate)
             << NV("units", lockShift.shiftUnits == UNIT_ARCSEC ? "arcsec/hr" : "pixels/hr")
             << NV("axes", lockShift.shiftIsMountCoords ? "RA/Dec" : "X/Y");
    }
    response << jrpc_result(rslt);
}

static bool get_double(double *d, const json_value *j)
{
    if (j->type == JSON_FLOAT)
    {
        *d = j->float_value;
        return true;
    }
    else if (j->type == JSON_INT)
    {
        *d = j->int_value;
        return true;
    }
    return false;
}

static bool parse_point(PHD_Point *pt, const json_value *j)
{
    if (j->type != JSON_ARRAY)
        return false;
    const json_value *jx = j->first_child;
    if (!jx)
        return false;
    const json_value *jy = jx->next_sibling;
    if (!jy || jy->next_sibling)
        return false;
    double x, y;
    if (!get_double(&x, jx) || !get_double(&y, jy))
        return false;
    pt->SetXY(x, y);
    return true;
}

static bool parse_lock_shift_params(LockPosShiftParams *shift, const json_value *params, wxString *error)
{
    // "params":[{"rate":[3.3,1.1],"units":"arcsec/hr","axes":"RA/Dec"}]
    // or
    // "params":{"rate":[3.3,1.1],"units":"arcsec/hr","axes":"RA/Dec"}

    if (params && params->type == JSON_ARRAY)
        params = params->first_child;

    Params p("rate", "units", "axes", params);

    shift->shiftUnits = UNIT_ARCSEC;
    shift->shiftIsMountCoords = true;

    const json_value *j;
    
    j = p.param("rate");
    if (!j || !parse_point(&shift->shiftRate, j))
    {
        *error = "expected rate value array";
        return false;
    }

    j = p.param("units");
    const char *units = j ? string_val(j) : "";

    if (wxStricmp(units, "arcsec/hr") == 0 ||
        wxStricmp(units, "arc-sec/hr") == 0)
    {
        shift->shiftUnits = UNIT_ARCSEC;
    }
    else if (wxStricmp(units, "pixels/hr") == 0)
    {
        shift->shiftUnits = UNIT_PIXELS;
    }
    else
    {
        *error = "expected units 'arcsec/hr' or 'pixels/hr'";
        return false;
    }

    j = p.param("axes");
    const char *axes = j ? string_val(j) : "";

    if (wxStricmp(axes, "RA/Dec") == 0)
    {
        shift->shiftIsMountCoords = true;
    }
    else if (wxStricmp(axes, "X/Y") == 0)
    {
        shift->shiftIsMountCoords = false;
    }
    else
    {
        *error = "expected axes 'RA/Dec' or 'X/Y'";
        return false;
    }

    return true;
}

static void set_lock_shift_params(JObj& response, const json_value *params)
{
    wxString err;
    LockPosShiftParams shift;
    if (!parse_lock_shift_params(&shift, params, &err))
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, err);
        return;
    }

    VERIFY_GUIDER(response);

    pFrame->pGuider->SetLockPosShiftRate(shift.shiftRate, shift.shiftUnits, shift.shiftIsMountCoords);

    response << jrpc_result(0);
}

static void save_image(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);

    if (!pFrame->pGuider->CurrentImage()->ImageData)
    {
        response << jrpc_error(2, "no image available");
        return;
    }

    wxString fname = wxFileName::CreateTempFileName(MyFrame::GetDefaultFileDir() + PATHSEPSTR + "save_image_");

    if (pFrame->pGuider->SaveCurrentImage(fname))
    {
        ::wxRemove(fname);
        response << jrpc_error(3, "error saving image");
        return;
    }

    JObj rslt;
    rslt << NV("filename", fname);
    response << jrpc_result(rslt);
}

static void get_use_subframes(JObj& response, const json_value *params)
{
    response << jrpc_result(pCamera && pCamera->UseSubframes);
}

static void get_search_region(JObj& response, const json_value *params)
{
    VERIFY_GUIDER(response);
    response << jrpc_result(pFrame->pGuider->GetSearchRegion());
}

struct B64Encode
{
    static const char *const E;
    std::ostringstream os;
    unsigned int t;
    size_t nread;

    B64Encode()
        : t(0), nread(0)
    {
    }
    void append1(unsigned char ch)
    {
        t <<= 8;
        t |= ch;
        if (++nread % 3 == 0)
        {
            os << E[t >> 18]
               << E[(t >> 12) & 0x3F]
               << E[(t >> 6) & 0x3F]
               << E[t & 0x3F];
            t = 0;
        }
    }
    void append(const void *src_, size_t len)
    {
        const unsigned char *src = (const unsigned char *) src_;
        const unsigned char *const end = src + len;
        while (src < end)
            append1(*src++);
    }
    std::string finish()
    {
        switch (nread % 3) {
        case 1:
            os << E[t >> 2]
               << E[(t & 0x3) << 4]
               << "==";
            break;
        case 2:
            os << E[t >> 10]
                << E[(t >> 4) & 0x3F]
                << E[(t & 0xf) << 2]
                << '=';
            break;
        }
        os << std::ends;
        return os.str();
    }
};
const char *const B64Encode::E = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void get_star_image(JObj& response, const json_value *params)
{
    int reqsize = 15;
    Params p("size", params);
    const json_value *val = p.param("size");
    if (val)
    {
        if (val->type != JSON_INT || (reqsize = val->int_value) < 15)
        {
            response << jrpc_error(JSONRPC_INVALID_PARAMS, "invalid image size param");
            return;
        }
    }

    VERIFY_GUIDER(response);

    Guider *guider = pFrame->pGuider;
    const usImage *img = guider->CurrentImage();
    const PHD_Point& star = guider->CurrentPosition();

    if (guider->GetState() < GUIDER_STATE::STATE_SELECTED || !img->ImageData || !star.IsValid())
    {
        response << jrpc_error(2, "no star selected");
        return;
    }

    int const halfw = wxMin((reqsize - 1) / 2, 31);
    int const fullw = 2 * halfw + 1;
    int const sx = (int) rint(star.X);
    int const sy = (int) rint(star.Y);
    wxRect rect(sx - halfw, sy - halfw, fullw, fullw);
    if (img->Subframe.IsEmpty())
        rect.Intersect(wxRect(img->Size));
    else
        rect.Intersect(img->Subframe);

    int width = rect.GetWidth();
    size_t size = width * rect.GetHeight() * sizeof(unsigned short);

    B64Encode enc;
    for (int y = rect.GetTop(); y <= rect.GetBottom(); y++)
    {
        const unsigned short *p = img->ImageData + y * img->Size.GetWidth() + rect.GetLeft();
        enc.append(p, rect.GetWidth() * sizeof(unsigned short));
    }

    PHD_Point pos(star);
    pos.X -= rect.GetLeft();
    pos.Y -= rect.GetTop();

    JObj rslt;
    rslt << NV("frame", (int) pFrame->m_frameCounter)
        << NV("width", rect.GetWidth())
        << NV("height", rect.GetHeight())
        << NV("star_pos", pos)
        << NV("pixels", enc.finish());

    response << jrpc_result(rslt);
}

static bool parse_settle(SettleParams *settle, const json_value *j, wxString *error)
{
    bool found_pixels = false, found_time = false, found_timeout = false;

    json_for_each (t, j)
    {
        if (float_param("pixels", t, &settle->tolerancePx))
        {
            found_pixels = true;
            continue;
        }
        double d;
        if (float_param("time", t, &d))
        {
            settle->settleTimeSec = (int) floor(d);
            found_time = true;
            continue;
        }
        if (float_param("timeout", t, &d))
        {
            settle->timeoutSec = (int) floor(d);
            found_timeout = true;
            continue;
        }
    }

    settle->frames = 99999;

    bool ok = found_pixels && found_time && found_timeout;
    if (!ok)
        *error = "invalid settle params";

    return ok;
}

static void guide(JObj& response, const json_value *params)
{
    // params:
    //   settle [object]:
    //     pixels [float]
    //     arcsecs [float]
    //     frames [integer]
    //     time [integer]
    //     timeout [integer]
    //   recalibrate: boolean
    //
    // {"method": "guide", "params": [{"pixels": 0.5, "time": 6, "timeout": 30}, false], "id": 42}
    //    or
    // {"method": "guide", "params": {"settle": {"pixels": 0.5, "time": 6, "timeout": 30}, "recalibrate": false}, "id": 42}
    //
    // todo:
    //   accept tolerance in arcsec or pixels
    //   accept settle time in seconds or frames

    SettleParams settle;

    Params p("settle", "recalibrate", params);
    const json_value *p0 = p.param("settle");
    if (!p0 || p0->type != JSON_OBJECT)
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected settle object param");
        return;
    }
    wxString errMsg;
    if (!parse_settle(&settle, p0, &errMsg))
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, errMsg);
        return;
    }

    bool recalibrate = false;
    const json_value *p1 = p.param("recalibrate");
    if (p1)
    {
        if (!bool_param(p1, &recalibrate))
        {
            response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected bool value for recalibrate");
            return;
        }
    }

    if (recalibrate && !pConfig->Global.GetBoolean("/server/guide_allow_recalibrate", true))
    {
        Debug.AddLine("ignoring client recalibration request since guide_allow_recalibrate = false");
        recalibrate = false;
    }

    wxString err;

    if (!PhdController::CanGuide(&err))
        response << jrpc_error(1, err);
    else if (PhdController::Guide(recalibrate, settle, &err))
        response << jrpc_result(0);
    else
        response << jrpc_error(1, err);
}

static void dither(JObj& response, const json_value *params)
{
    // params:
    //   amount [integer] - max pixels to move in each axis
    //   raOnly [bool] - when true, only dither ra
    //   settle [object]:
    //     pixels [float]
    //     arcsecs [float]
    //     frames [integer]
    //     time [integer]
    //     timeout [integer]
    //
    // {"method": "dither", "params": [10, false, {"pixels": 1.5, "time": 8, "timeout": 30}], "id": 42}
    //    or
    // {"method": "dither", "params": {"amount": 10, "raOnly": false, "settle": {"pixels": 1.5, "time": 8, "timeout": 30}}, "id": 42}

    Params p("amount", "raOnly", "settle", params);
    const json_value *jv;
    double ditherAmt;

    jv = p.param("amount");
    if (!jv || !float_param(jv, &ditherAmt))
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected dither amount param");
        return;
    }

    bool raOnly = false;
    jv = p.param("raOnly");
    if (jv)
    {
        if (!bool_param(jv, &raOnly))
        {
            response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected dither raOnly param");
            return;
        }
    }

    SettleParams settle;

    jv = p.param("settle");
    if (!jv || jv->type != JSON_OBJECT)
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, "expected settle object param");
        return;
    }
    wxString errMsg;
    if (!parse_settle(&settle, jv, &errMsg))
    {
        response << jrpc_error(JSONRPC_INVALID_PARAMS, errMsg);
        return;
    }

    wxString error;
    if (PhdController::Dither(fabs(ditherAmt), raOnly, settle, &error))
        response << jrpc_result(0);
    else
        response << jrpc_error(1, error);
}

static void shutdown(JObj& response, const json_value *params)
{
#if defined(__WINDOWS__)

    // The wxEVT_CLOSE_WINDOW message may not be processed on Windows if phd2 is sitting idle
    // when the client invokes shutdown. As a workaround pump some timer event messages to
    // keep the event loop from stalling and ensure that the wxEVT_CLOSE_WINDOW is processed.

    (new wxTimer(&wxGetApp()))->Start(20); // this object leaks but we don't care
#endif

    wxCloseEvent *evt = new wxCloseEvent(wxEVT_CLOSE_WINDOW);
    evt->SetCanVeto(false);
    wxQueueEvent(pFrame, evt);

    response << jrpc_result(0);
}

static void get_camera_binning(JObj& response, const json_value *params)
{
    if (pCamera && pCamera->Connected)
    {
        int binning = pCamera->Binning;
        response << jrpc_result(binning);
    }
    else
        response << jrpc_error(1, "camera not connected");
}

static void dump_request(const wxSocketClient *cli, const json_value *req)
{
    Debug.Write(wxString::Format("evsrv: cli %p request: %s\n", cli, json_format(req)));
}

static void dump_response(const wxSocketClient *cli, const JRpcResponse& resp)
{
    Debug.Write(wxString::Format("evsrv: cli %p response: %s\n", cli, const_cast<JRpcResponse&>(resp).str()));
}

static bool handle_request(const wxSocketClient *cli, JObj& response, const json_value *req)
{
    const json_value *method;
    const json_value *params;
    const json_value *id;

    dump_request(cli, req);

    parse_request(req, &method, &params, &id);

    if (!method)
    {
        response << jrpc_error(JSONRPC_INVALID_REQUEST, "invalid request") << jrpc_id(0);
        return true;
    }

    static struct {
        const char *name;
        void (*fn)(JObj& response, const json_value *params);
    } methods[] = {
        { "clear_calibration", &clear_calibration, },
        { "deselect_star", &deselect_star, },
        { "get_exposure", &get_exposure, },
        { "set_exposure", &set_exposure, },
        { "get_exposure_durations", &get_exposure_durations, },
        { "get_profiles", &get_profiles, },
        { "get_profile", &get_profile, },
        { "set_profile", &set_profile, },
        { "get_connected", &get_connected, },
        { "set_connected", &set_connected, },
        { "get_calibrated", &get_calibrated, },
        { "get_paused", &get_paused, },
        { "set_paused", &set_paused, },
        { "get_lock_position", &get_lock_position, },
        { "set_lock_position", &set_lock_position, },
        { "loop", &loop, },
        { "stop_capture", &stop_capture, },
        { "guide", &guide, },
        { "dither", &dither, },
        { "find_star", &find_star, },
        { "get_pixel_scale", &get_pixel_scale, },
        { "get_app_state", &get_app_state, },
        { "flip_calibration", &flip_calibration, },
        { "get_lock_shift_enabled", &get_lock_shift_enabled, },
        { "set_lock_shift_enabled", &set_lock_shift_enabled, },
        { "get_lock_shift_params", &get_lock_shift_params, },
        { "set_lock_shift_params", &set_lock_shift_params, },
        { "save_image", &save_image, },
        { "get_star_image", &get_star_image, },
        { "get_use_subframes", &get_use_subframes, },
        { "get_search_region", &get_search_region, },
        { "shutdown", &shutdown, },
        { "get_camera_binning", &get_camera_binning, },
    };

    for (unsigned int i = 0; i < WXSIZEOF(methods); i++)
    {
        if (strcmp(method->string_value, methods[i].name) == 0)
        {
            (*methods[i].fn)(response, params);
            if (id)
            {
                response << jrpc_id(id);
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    if (id)
    {
        response << jrpc_error(JSONRPC_METHOD_NOT_FOUND, "method not found") << jrpc_id(id);
        return true;
    }
    else
    {
        return false;
    }
}

static void handle_cli_input_complete(wxSocketClient *cli, char *input, JsonParser& parser)
{
    if (!parser.Parse(input))
    {
        JRpcResponse response;
        response << jrpc_error(JSONRPC_PARSE_ERROR, parser_error(parser)) << jrpc_id(0);
        dump_response(cli, response);
        do_notify1(cli, response);
        return;
    }

    const json_value *root = parser.Root();

    if (root->type == JSON_ARRAY)
    {
        // a batch request

        JAry ary;

        bool found = false;
        json_for_each (req, root)
        {
            JRpcResponse response;
            if (handle_request(cli, response, req))
            {
                dump_response(cli, response);
                ary << response;
                found = true;
            }
        }

        if (found)
            do_notify1(cli, ary);
    }
    else
    {
        // a single request

        const json_value *const req = root;
        JRpcResponse response;
        if (handle_request(cli, response, req))
        {
            dump_response(cli, response);
            do_notify1(cli, response);
        }
    }
}

static void handle_cli_input(wxSocketClient *cli, JsonParser& parser)
{
    // Bump refcnt to protect against reentrancy.
    //
    // Some functions like set_connected can cause the event loop to run reentrantly. If the
    // client disconnects before the response is sent and a socket disconnect event is
    // dispatched the client data could be destroyed before we respond.

    ClientDataGuard clidata(cli);

    ClientReadBuf *rdbuf = &clidata->rdbuf;

    wxSocketInputStream sis(*cli);
    size_t avail = rdbuf->avail();

    while (sis.CanRead())
    {
        if (avail == 0)
        {
            drain_input(sis);

            JRpcResponse response;
            response << jrpc_error(JSONRPC_INTERNAL_ERROR, "too big") << jrpc_id(0);
            do_notify1(cli, response);

            rdbuf->reset();
            break;
        }
        size_t n = sis.Read(rdbuf->dest, avail).LastRead();
        if (n == 0)
            break;

        if (find_eol(rdbuf->dest, n))
        {
            drain_input(sis);
            handle_cli_input_complete(cli, &rdbuf->buf[0], parser);
            rdbuf->reset();
            break;
        }

        rdbuf->dest += n;
        avail -= n;
    }
}

EventServer::EventServer()
{
}

EventServer::~EventServer()
{
}

bool EventServer::EventServerStart(unsigned int instanceId)
{
    if (m_serverSocket)
    {
        Debug.AddLine("attempt to start event server when it is already started?");
        return false;
    }

    unsigned int port = 4400 + instanceId - 1;
    wxIPV4address eventServerAddr;
    eventServerAddr.Service(port);
    m_serverSocket = new wxSocketServer(eventServerAddr);

    if (!m_serverSocket->Ok())
    {
        Debug.Write(wxString::Format("Event server failed to start - Could not listen at port %u\n", port));
        delete m_serverSocket;
        m_serverSocket = NULL;
        return true;
    }

    m_serverSocket->SetEventHandler(*this, EVENT_SERVER_ID);
    m_serverSocket->SetNotify(wxSOCKET_CONNECTION_FLAG);
    m_serverSocket->Notify(true);

    Debug.Write(wxString::Format("event server started, listening on port %u\n", port));

    return false;
}

void EventServer::EventServerStop()
{
    if (!m_serverSocket)
        return;

    for (CliSockSet::const_iterator it = m_eventServerClients.begin();
         it != m_eventServerClients.end(); ++it)
    {
        destroy_client(*it);
    }
    m_eventServerClients.clear();

    delete m_serverSocket;
    m_serverSocket = NULL;

    Debug.AddLine("event server stopped");
}

void EventServer::OnEventServerEvent(wxSocketEvent& event)
{
    wxSocketServer *server = static_cast<wxSocketServer *>(event.GetSocket());

    if (event.GetSocketEvent() != wxSOCKET_CONNECTION)
        return;

    wxSocketClient *client = static_cast<wxSocketClient *>(server->Accept(false));

    if (!client)
        return;

    Debug.Write(wxString::Format("evsrv: cli %p connect\n", client));

    client->SetEventHandler(*this, EVENT_SERVER_CLIENT_ID);
    client->SetNotify(wxSOCKET_LOST_FLAG | wxSOCKET_INPUT_FLAG);
    client->SetFlags(wxSOCKET_NOWAIT);
    client->Notify(true);
    client->SetClientData(new ClientData(client));

    send_catchup_events(client);

    m_eventServerClients.insert(client);
}

void EventServer::OnEventServerClientEvent(wxSocketEvent& event)
{
    wxSocketClient *cli = static_cast<wxSocketClient *>(event.GetSocket());

    if (event.GetSocketEvent() == wxSOCKET_LOST)
    {
        Debug.Write(wxString::Format("evsrv: cli %p disconnect\n", cli));

        unsigned int const n = m_eventServerClients.erase(cli);
        if (n != 1)
            Debug.AddLine("client disconnected but not present in client set!");

        destroy_client(cli);
    }
    else if (event.GetSocketEvent() == wxSOCKET_INPUT)
    {
        handle_cli_input(cli, m_parser);
    }
    else
    {
        Debug.Write(wxString::Format("unexpected client socket event %d\n", event.GetSocketEvent()));
    }
}

void EventServer::NotifyStartCalibration(Mount *mount)
{
    SIMPLE_NOTIFY_EV(ev_start_calibration(mount));
}

void EventServer::NotifyCalibrationFailed(Mount *mount, const wxString& msg)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev("CalibrationFailed");
    ev << NVMount(mount) << NV("Reason", msg);

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifyCalibrationComplete(Mount *mount)
{
    if (m_eventServerClients.empty())
        return;

    do_notify(m_eventServerClients, ev_calibration_complete(mount));
}

void EventServer::NotifyCalibrationDataFlipped(Mount *mount)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev("CalibrationDataFlipped");
    ev << NVMount(mount);

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifyLooping(unsigned int exposure)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev("LoopingExposures");
    ev << NV("Frame", (int) exposure);

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifyLoopingStopped()
{
    SIMPLE_NOTIFY("LoopingExposuresStopped");
}

void EventServer::NotifyStarSelected(const PHD_Point& pt)
{
    SIMPLE_NOTIFY_EV(ev_star_selected(pt));
}

void EventServer::NotifyStarLost(const FrameDroppedInfo& info)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev("StarLost");

    ev << NV("Frame", info.frameNumber)
       << NV("Time", info.time, 3)
       << NV("StarMass", info.starMass, 0)
       << NV("SNR", info.starSNR, 2)
       << NV("AvgDist", info.avgDist, 2);

    if (info.starError)
        ev << NV("ErrorCode", info.starError);

    if (!info.status.IsEmpty())
        ev << NV("Status", info.status);

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifyStartGuiding()
{
    SIMPLE_NOTIFY_EV(ev_start_guiding());
}

void EventServer::NotifyGuidingStopped()
{
    SIMPLE_NOTIFY("GuidingStopped");
}

void EventServer::NotifyPaused()
{
    SIMPLE_NOTIFY_EV(ev_paused());
}

void EventServer::NotifyResumed()
{
    SIMPLE_NOTIFY("Resumed");
}

void EventServer::NotifyGuideStep(const GuideStepInfo& step)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev("GuideStep");

    ev << NV("Frame", step.frameNumber)
       << NV("Time", step.time, 3)
       << NVMount(step.mount)
       << NV("dx", step.cameraOffset.X, 3)
       << NV("dy", step.cameraOffset.Y, 3)
       << NV("RADistanceRaw", step.mountOffset.X, 3)
       << NV("DECDistanceRaw", step.mountOffset.Y, 3)
       << NV("RADistanceGuide", step.guideDistanceRA, 3)
       << NV("DECDistanceGuide", step.guideDistanceDec, 3);

    if (step.durationRA > 0)
    {
       ev << NV("RADuration", step.durationRA)
          << NV("RADirection", step.mount->DirectionStr((GUIDE_DIRECTION)step.directionRA));
    }

    if (step.durationDec > 0)
    {
        ev << NV("DECDuration", step.durationDec)
           << NV("DECDirection", step.mount->DirectionStr((GUIDE_DIRECTION)step.directionDec));
    }

    if (step.mount->IsStepGuider())
    {
        ev << NV("Pos", step.aoPos);
    }

    ev << NV("StarMass", step.starMass, 0)
       << NV("SNR", step.starSNR, 2)
       << NV("AvgDist", step.avgDist, 2);

    if (step.starError)
       ev << NV("ErrorCode", step.starError);

    if (step.raLimited)
        ev << NV("RALimited", true);

    if (step.decLimited)
        ev << NV("DecLimited", true);

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifyGuidingDithered(double dx, double dy)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev("GuidingDithered");
    ev << NV("dx", dx, 3) << NV("dy", dy, 3);

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifySetLockPosition(const PHD_Point& xy)
{
    if (m_eventServerClients.empty())
        return;

    do_notify(m_eventServerClients, ev_set_lock_position(xy));
}

void EventServer::NotifyLockPositionLost()
{
    SIMPLE_NOTIFY("LockPositionLost");
}

void EventServer::NotifyAppState()
{
    if (m_eventServerClients.empty())
        return;

    do_notify(m_eventServerClients, ev_app_state());
}

void EventServer::NotifySettling(double distance, double time, double settleTime)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev(ev_settling(distance, time, settleTime));

    Debug.Write(wxString::Format("evsrv: %s\n", ev.str()));

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifySettleDone(const wxString& errorMsg)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev(ev_settle_done(errorMsg));

    Debug.Write(wxString::Format("evsrv: %s\n", ev.str()));

    do_notify(m_eventServerClients, ev);
}

void EventServer::NotifyAlert(const wxString& msg, int type)
{
    if (m_eventServerClients.empty())
        return;

    Ev ev("Alert");
    ev << NV("Msg", msg);

    wxString s;
    switch (type)
    {
    case wxICON_NONE:
    case wxICON_INFORMATION:
    default:
        s = "info";
        break;
    case wxICON_QUESTION:
        s = "question";
        break;
    case wxICON_WARNING:
        s = "warning";
        break;
    case wxICON_ERROR:
        s = "error";
        break;
    }
    ev << NV("Type", s);

    do_notify(m_eventServerClients, ev);
}
