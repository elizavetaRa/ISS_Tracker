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
OsmGpsMapLayer *osd;
GtkWidget *iss_location_label;
GObject *recenter_button;
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

                    // Update label with current ISS location
                    gchar *location_text = g_strdup_printf("Current ISS Location: Latitude: %f, Longitude: %f", float_lat, float_lon);
                    gtk_label_set_text(GTK_LABEL(iss_location_label), location_text);
                    g_free(location_text);
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
        osm_gps_map_set_zoom(OSM_GPS_MAP(map), 3);
        g_object_set(track, "editable", FALSE, NULL);
        osm_gps_map_track_add(OSM_GPS_MAP(map), track);
    }

    // add newly retrieved tracking point to the map
    point = osm_gps_map_point_new_degrees(float_lat,
                                          float_lon);
    osm_gps_map_track_add_point(track, point);

    return TRUE;
}

// Recenter the map to the current ISS location
static void recenter_map(GtkWidget *widget, gpointer data)
{
    osm_gps_map_set_center(OSM_GPS_MAP(map), float_lat, float_lon);
}

static void activate(GtkApplication *app, gpointer user_data)
{
    GtkBuilder *builder;
    GtkWidget *window;
    GtkWidget *map_container;
    GError *error = NULL;

    // Construct a GtkBuilder instance and load our UI description
    builder = gtk_builder_new();
    if (gtk_builder_add_from_file(builder, "builder.ui", &error) == 0)
    {
        g_printerr("Error loading file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    // Create UI elements
    window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    map_container = GTK_WIDGET(gtk_builder_get_object(builder, "map_container"));
    iss_location_label = GTK_WIDGET(gtk_builder_get_object(builder, "iss_location_label"));

    if (!GTK_IS_CONTAINER(map_container))
    {
        g_printerr("map_container is not a valid GTK container\n");
        return;
    }

    map = osm_gps_map_new();
    gtk_widget_set_hexpand(map, TRUE);
    gtk_widget_set_vexpand(map, TRUE);
    gtk_container_add(GTK_CONTAINER(map_container), map);

    osd = g_object_new(OSM_TYPE_GPS_MAP_OSD,
                       "show-scale", TRUE,
                       "show-coordinates", FALSE,
                       "show-crosshair", TRUE,
                       "show-dpad", FALSE,
                       "show-zoom", TRUE,
                       "show-gps-in-dpad", FALSE,
                       "show-gps-in-zoom", FALSE,
                       "dpad-radius", 30,
                       NULL);
    osm_gps_map_layer_add(OSM_GPS_MAP(map), osd);
    g_object_unref(G_OBJECT(osd));

    recenter_button = gtk_builder_get_object(builder, "recenter_button");
    g_signal_connect(recenter_button, "clicked", G_CALLBACK(recenter_map), NULL);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    // Initially call ISS location API and then call every 5 seconds
    addIssLocationPoint();
    g_timeout_add_seconds(5, addIssLocationPoint, NULL);
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    int status;

    gtk_init(&argc, &argv);

    app = gtk_application_new("com.example.iss-tracker", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    gtk_main();
    return 0;
}