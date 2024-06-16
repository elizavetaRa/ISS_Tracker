#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <stdlib.h>

#include "osm-gps-map.h"

float float_lat;

float float_lon;
OsmGpsMapTrack *track;
OsmGpsMapPoint *point;
GtkWidget *map;
static char *response = NULL;

// Callback function to handle CURL response
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    char **response_ptr = (char **)userp;

    *response_ptr = strndup(contents, realsize);

    return realsize;
}

// API Call to get the current latitude and the longitude of ISS
static gboolean getIssLocation()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (curl)
    {
        CURLcode res;

        curl_easy_setopt(curl, CURLOPT_URL, "http://api.open-notify.org/iss-now.json");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);

        if (res == CURLE_OK && response)
        {
            struct json_object *parsed_json;
            struct json_object *iss_position;
            struct json_object *latitude;
            struct json_object *longitude;

            parsed_json = json_tokener_parse(response);
            if (parsed_json == NULL)
            {
                fprintf(stderr, "Failed to parse JSON response\n");
            }

            if (json_object_object_get_ex(parsed_json, "iss_position", &iss_position))
            {
                if (json_object_object_get_ex(iss_position, "latitude", &latitude) &&
                    json_object_object_get_ex(iss_position, "longitude", &longitude))
                {

                    // Get latitude and longitude as strings
                    const char *lat_str = json_object_get_string(latitude);
                    const char *lon_str = json_object_get_string(longitude);

                    // Convert latitude and longitude to gdouble (double precision)
                    gdouble lat = g_ascii_strtod(lat_str, NULL);
                    gdouble lon = g_ascii_strtod(lon_str, NULL);

                    // Convert gdouble to float
                    float_lat = (float)lat;
                    float_lon = (float)lon;

                    printf("Float Latitude: %f, Float Longitude: %f\n", float_lat, float_lon);

                    // Example usage in your application context (not included in this snippet)
                    // osm_gps_map_set_center_and_zoom(OSM_GPS_MAP(widgets->map), lat, lon, 2);
                }
                else
                {
                    fprintf(stderr, "Failed to get latitude or longitude from JSON\n");
                }
            }
            else
            {
                fprintf(stderr, "Failed to get iss_position object from JSON\n");
            }

            json_object_put(parsed_json);
            free(response);
            response = NULL; // Reset response pointer after freeing memory
        }
        else
        {
            fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }
    else
    {
        fprintf(stderr, "Failed to initialize CURL\n");
    }

    return TRUE;
}

// Function to create a new map track with ISS location
// or add a new location point to an excisting track
static gboolean addIssLocationPoint()
{
    getIssLocation();

    // if no track is created, create new and set the starting tracking point as map center
    if (!track)
    {
        track = osm_gps_map_track_new();
        osm_gps_map_set_center(OSM_GPS_MAP(map), float_lat, float_lon);
    }

    point = osm_gps_map_point_new_degrees(float_lat,
                                          float_lon);
    osm_gps_map_track_add_point(track, point);

    g_object_set(track, "editable", FALSE, NULL);

    osm_gps_map_track_add(OSM_GPS_MAP(map), track);

    return TRUE;
}

static void activate(GtkApplication *app, gpointer user_data)
{

    GtkWidget *window;

    window = gtk_application_window_new(app);
    map = osm_gps_map_new();

    osm_gps_map_set_zoom(OSM_GPS_MAP(map), 4);
    gtk_container_add(GTK_CONTAINER(window), map);

    gtk_window_set_title(GTK_WINDOW(window), "ISS Live Location");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_widget_show_all(window);

    addIssLocationPoint();
    g_timeout_add_seconds(5, addIssLocationPoint, window);
}

int main(int argc, char **argv)
{

    GtkApplication *app;
    int status;

    app = gtk_application_new("com.example.iss-tracker", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    gtk_main();
    return 0;
}