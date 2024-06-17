#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>

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
    response = strndup(contents, realsize);
    return realsize;
}

// Function to perform the CURL request to ISS API in a separate thread
static void getIssApiDataTask(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "http://api.open-notify.org/iss-now.json");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Add timeout of 5 seconds

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            g_task_return_error(task, g_error_new_literal(G_IO_ERROR, G_IO_ERROR_FAILED, curl_easy_strerror(res)));
        }
        else
        {
            g_task_return_pointer(task, response, g_free);
        }

        curl_easy_cleanup(curl);
    }
    else
    {
        g_task_return_error(task, g_error_new_literal(G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to initialize CURL"));
    }

    curl_global_cleanup();
}

// Function to handle the completion of the async task
static void onIssApiDataTaskComplete(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GTask *task = G_TASK(res);
    if (g_task_had_error(task))
    {
        g_warning("API request failed");
    }
    else
    {
        char *response = g_task_propagate_pointer(task, NULL);
        if (response)
        {
            struct json_object *parsed_json = json_tokener_parse(response);
            if (parsed_json == NULL)
            {
                fprintf(stderr, "Failed to parse JSON response\n");
                return;
            }

            struct json_object *iss_position;
            struct json_object *latitude;
            struct json_object *longitude;

            if (json_object_object_get_ex(parsed_json, "iss_position", &iss_position))
            {
                if (json_object_object_get_ex(iss_position, "latitude", &latitude) &&
                    json_object_object_get_ex(iss_position, "longitude", &longitude))
                {
                    const char *lat_str = json_object_get_string(latitude);
                    const char *lon_str = json_object_get_string(longitude);

                    gdouble lat = g_ascii_strtod(lat_str, NULL);
                    gdouble lon = g_ascii_strtod(lon_str, NULL);

                    float_lat = (float)lat;
                    float_lon = (float)lon;

                    printf("Float Latitude: %f, Float Longitude: %f\n", float_lat, float_lon);

                    if (track)
                    {
                        // Update existing track
                        osm_gps_map_track_add_point(track, osm_gps_map_point_new_degrees(float_lat, float_lon));
                    }
                    else
                    {
                        // Create new track if not already created
                        track = osm_gps_map_track_new();
                        osm_gps_map_set_center(OSM_GPS_MAP(map), float_lat, float_lon);
                        osm_gps_map_set_zoom(OSM_GPS_MAP(map), 3);
                        g_object_set(track, "editable", FALSE, NULL);
                        osm_gps_map_track_add(OSM_GPS_MAP(map), track);
                    }

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
            g_free(response);
        }
    }
}

// Function to initiate the async API call
static gboolean getIssLocationAsync(gpointer user_data)
{
    GTask *task = g_task_new(NULL, NULL, onIssApiDataTaskComplete, NULL);

    // Runs task in another thread and when it return, callback will be invoced in GMainContext
    g_task_run_in_thread(task, getIssApiDataTask);
    g_object_unref(task);
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
    getIssLocationAsync(NULL);
    g_timeout_add_seconds(5, getIssLocationAsync, NULL);
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