#include "application.h"

int
main(int argc, char **argv)
{
    GswordApplication *application = gsword_application_new();
    int status;

    status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    return status;
}
