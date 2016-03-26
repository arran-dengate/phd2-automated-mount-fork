#include <stdio.h>
#include "openssag.h"
using namespace OpenSSAG;

int main()
{
    OpenSSAG::SSAG *camera = new OpenSSAG::SSAG();
    if (camera->Connect()) {
        struct raw_image *image = camera->Expose(1000);
        FILE *fp = fopen("image", "w");
        printf("Saving image");
        fwrite(image->data, 1, image->width * image->height, fp);
        fclose(fp);
        camera->Disconnect();
        printf("Successfully completed\n");
    }
    else {
        printf("Could not find StarShoot Autoguider\n");
    }
}