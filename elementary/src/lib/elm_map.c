#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#include "Elementary.h"
#include "elm_priv.h"
#include "els_scroller.h"

#ifdef HAVE_ELEMENTARY_ECORE_CON

typedef struct _Widget_Data Widget_Data;
typedef struct _Pan Pan;
typedef struct _Grid Grid;
typedef struct _Grid_Item Grid_Item;
typedef struct _Marker_Group Marker_Group;
typedef struct _Marker_Bubble Marker_Bubble;
typedef struct _Path_Node Path_Node;
typedef struct _Path_Waypoint Path_Waypoint;
typedef struct _Url_Data Url_Data;
typedef struct _Route_Dump Route_Dump;
typedef struct _Name_Dump Name_Dump;
typedef struct _Track_Dump Track_Dump;
typedef struct _Delayed_Data Delayed_Data;
typedef struct _Map_Sources_Tab Map_Sources_Tab;

#define ROUND(z) (((z) < 0) ? (int)ceil((z) - 0.005) : (int)floor((z) + 0.005))
#define EVAS_MAP_POINT 4
#define DEFAULT_TILE_SIZE 256
#define MARER_MAX_NUMBER 30
#define CACHE_ROOT_PATH   "/tmp/elm_map"
#define CACHE_PATH        CACHE_ROOT_PATH"/%d/%d/%d"
#define CACHE_FILE_PATH   "%s/%d.png"
#define DEST_ROUTE_XML_FILE "/tmp/elm_map-route-XXXXXX"
#define DEST_NAME_XML_FILE "/tmp/elm_map-name-XXXXXX"

#define ROUTE_YOURS_URL "http://www.yournavigation.org/api/dev/route.php"
#define ROUTE_TYPE_MOTORCAR "motocar"
#define ROUTE_TYPE_BICYCLE "bicycle"
#define ROUTE_TYPE_FOOT "foot"
#define YOURS_DISTANCE "distance"
#define YOURS_DESCRIPTION "description"
#define YOURS_COORDINATES "coordinates"

#define NAME_NOMINATIM_URL "http://nominatim.openstreetmap.org"
#define NOMINATIM_RESULT "result"
#define NOMINATIM_PLACE "place"
#define NOMINATIM_ATTR_LON "lon"
#define NOMINATIM_ATTR_LAT "lat"

#define MAX_CONCURRENT_DOWNLOAD 10

/* FIXME: This is unused currently
#define GPX_NAME "name>"
#define GPX_COORDINATES "trkpt "
#define GPX_LON "lon"
#define GPX_LAT "lat"
#define GPX_ELE "ele>"
#define GPX_TIME "time>"
*/

enum _Route_Xml_Attribute
{
   ROUTE_XML_NONE,
   ROUTE_XML_DISTANCE,
   ROUTE_XML_DESCRIPTION,
   ROUTE_XML_COORDINATES,
   ROUTE_XML_LAST
} Route_Xml_Attibute;

enum _Name_Xml_Attribute
{
   NAME_XML_NONE,
   NAME_XML_NAME,
   NAME_XML_LON,
   NAME_XML_LAT,
   NAME_XML_LAST
} Name_Xml_Attibute;

enum _Track_Xml_Attribute
{
   TRACK_XML_NONE,
   TRACK_XML_COORDINATES,
   TRACK_XML_LAST
} Track_Xml_Attibute;

struct _Delayed_Data
{
   void (*func)(void *data);
   Widget_Data *wd;
   Elm_Map_Zoom_Mode mode;
   int zoom;
   double lon, lat;
   Eina_List *markers;
};

// Map sources
// Currently the size of a tile must be 256*256
// and the size of the map must be pow(2.0, z)*tile_size
struct _Map_Sources_Tab
{
   const char *name;
   int zoom_min;
   int zoom_max;
   ElmMapModuleUrlFunc url_cb;
   Elm_Map_Route_Sources route_source;
   ElmMapModuleRouteUrlFunc route_url_cb;
   ElmMapModuleNameUrlFunc name_url_cb;
   ElmMapModuleGeoIntoCoordFunc geo_into_coord;
   ElmMapModuleCoordIntoGeoFunc coord_into_geo;
};

struct _Url_Data
{
   Ecore_Con_Url *con_url;

   FILE *fd;
   char *fname;
};

struct _Elm_Map_Marker_Class
{
   const char *style;
   struct _Elm_Map_Marker_Class_Func
     {
        ElmMapMarkerGetFunc get;
        ElmMapMarkerDelFunc del; //if NULL the object will be destroyed with evas_object_del()
        ElmMapMarkerIconGetFunc icon_get;
     } func;
};

struct _Elm_Map_Group_Class
{
   Widget_Data *wd;

   Eina_List *markers;
   int zoom_displayed; // display the group if the zoom is >= to zoom_display
   int zoom_grouped;   // group the markers only if the zoom is <= to zoom_groups
   const char *style;
   void *data;
   struct
     {
        ElmMapGroupIconGetFunc icon_get;
     } func;

   Eina_Bool hide : 1;
};

struct _Marker_Bubble
{
   Widget_Data *wd;
   Evas_Object *pobj;
   Evas_Object *obj, *sc, *bx;
};

struct _Elm_Map_Marker
{
   Widget_Data *wd;
   Elm_Map_Marker_Class *clas;
   Elm_Map_Group_Class *group_clas;
   double longitude, latitude;
   Evas_Coord w, h;
   Evas_Object *obj;

   Evas_Coord x, y;
   Eina_Bool grouped : 1;
   Eina_Bool leader : 1; // if marker is group leader
   Marker_Group *group;

   Marker_Bubble *bubble;
   Evas_Object *content;
   void *data;
};

struct _Marker_Group
{
   Widget_Data *wd;
   Elm_Map_Group_Class *clas;
   Evas_Coord w, h;
   Evas_Object *obj;

   Evas_Coord x, y;
   Eina_List *markers;

   Marker_Bubble *bubble;
};

struct _Elm_Map_Route
{
   Widget_Data *wd;

   Path_Node *n;
   Path_Waypoint *w;
   Ecore_Con_Url *con_url;

   int type;
   int method;
   int x, y;
   double flon, flat, tlon, tlat;

   Eina_List *nodes, *path;
   Eina_List *waypoint;

   struct
     {
        int node_count;
        int waypoint_count;
        const char *nodes;
        const char *waypoints;
        double distance; /* unit : km */
     } info;

   Eina_List *handlers;
   Url_Data ud;

   struct
     {
        int r;
        int g;
        int b;
        int a;
     } color;

   Eina_Bool inbound : 1;
};

struct _Path_Node
{
   Widget_Data *wd;

   int idx;
   struct
     {
        double lon, lat;
        char *address;
     } pos;
};

struct _Path_Waypoint
{
   Widget_Data *wd;

   const char *point;
};

struct _Elm_Map_Name
{
   Widget_Data *wd;

   Ecore_Con_Url *con_url;
   int method;
   char *address;
   double lon, lat;
   Url_Data ud;
   Ecore_Event_Handler *handler;
};

struct _Route_Dump
{
   int id;
   char *fname;
   double distance;
   char *description;
   char *coordinates;
};

struct _Name_Dump
{
   int id;
   char *address;
   double lon;
   double lat;
};

struct _Grid_Item
{
   Grid *g;

   Widget_Data *wd;
   Evas_Object *img;
   const char *file;
   const char *source;
   int x, y;  // Tile coordinate
   Eina_Bool file_have : 1;

   Ecore_File_Download_Job *job;
};

struct _Grid
{
   Widget_Data *wd;
   int zoom; // zoom level tiles want for optimal display (1, 2, 4, 8)
   int tw, th; // size of grid in tiles
   Eina_Matrixsparse *grid;
};

struct _Pan
{
   Evas_Object_Smart_Clipped_Data __clipped_data;
   Widget_Data *wd;
};

struct _Widget_Data
{
   Evas_Object *obj;
   Evas_Object *scr;
   Evas_Object *ges;
   Evas_Object *pan_smart;
   Evas_Object *sep_maps_markers; // Tiles are below this and overlays are on top
   Evas_Map *map;

   Map_Sources_Tab *src;
   Eina_List *srcs;
   const char **src_names;
   int zoom_min, zoom_max;
   int tsize;

   int id;
   Eina_List *grids;

   int zoom;
   double zoom_detail;
   double prev_lon, prev_lat;
   Evas_Coord ox, oy;
   struct
     {
        int w, h;  // Current pixel width, heigth of a grid
        int tile;  // Current pixel size of a grid item
     } size;
   Elm_Map_Zoom_Mode mode;
   struct
     {
        double zoom;
        double diff;
        int cnt;
     } ani;
   Ecore_Timer *zoom_timer;
   Ecore_Animator *zoom_animator;

   int try_num;
   int finish_num;
   int download_num;
   Eina_List *download_list;
   Ecore_Idler *download_idler;
   Eina_Hash *ua;
   const char *user_agent;

   Evas_Coord pan_x, pan_y;
   Eina_List *delayed_jobs;

   Ecore_Timer *scr_timer;
   Ecore_Timer *long_timer;
   Evas_Event_Mouse_Down ev;
   Eina_Bool on_hold : 1;
   Eina_Bool paused : 1;

   double pinch_zoom;
   struct
     {
        Evas_Coord cx, cy;
        double a, d;
     } rotate;

   Eina_Bool wheel_disabled : 1;

   unsigned int markers_max_num;
   Eina_Bool paused_markers : 1;
   Eina_List *group_classes;
   Eina_List *marker_classes;
   Eina_List *markers;

   Elm_Map_Route_Sources route_source;
   Eina_List *route;
   Eina_List *track;
   Eina_List *names;
};

static char *_mapnik_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom);
static char *_osmarender_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom);
static char *_cyclemap_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom);
static char *_mapquest_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom);
static char *_mapquest_aerial_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom);
static char *_yours_url_cb(Evas_Object *obj __UNUSED__, char *type_name, int method, double flon, double flat, double tlon, double tlat);
static char *_nominatim_url_cb(Evas_Object *obj, int method, char *name, double lon, double lat);
/*
static char *_monav_url_cb(Evas_Object *obj __UNUSED__, char *type_name, int method, double flon, double flat, double tlon, double tlat)
static char *_ors_url_cb(Evas_Object *obj __UNUSED__, char *type_name, int method, double flon, double flat, double tlon, double tlat);
*/

static Map_Sources_Tab default_map_sources_tab[] =
{
     {"Mapnik", 0, 18, _mapnik_url_cb, ELM_MAP_ROUTE_SOURCE_YOURS, _yours_url_cb, _nominatim_url_cb, NULL, NULL},
     {"Osmarender", 0, 17, _osmarender_url_cb, ELM_MAP_ROUTE_SOURCE_YOURS, _yours_url_cb, _nominatim_url_cb, NULL, NULL},
     {"CycleMap", 0, 16, _cyclemap_url_cb, ELM_MAP_ROUTE_SOURCE_YOURS, _yours_url_cb, _nominatim_url_cb, NULL, NULL},
     {"MapQuest", 0, 18, _mapquest_url_cb, ELM_MAP_ROUTE_SOURCE_YOURS, _yours_url_cb, _nominatim_url_cb, NULL, NULL},
     {"MapQuest Open Aerial", 0, 11, _mapquest_aerial_url_cb, ELM_MAP_ROUTE_SOURCE_YOURS, _yours_url_cb, _nominatim_url_cb, NULL, NULL},
};

static const char *widtype = NULL;
static Evas_Smart_Class parent_sc = EVAS_SMART_CLASS_INIT_NULL;
static Evas_Smart_Class sc;
static Evas_Smart *smart;
static int idnum = 1;

static const char SIG_CHANGED[] = "changed";
static const char SIG_CLICKED[] = "clicked";
static const char SIG_CLICKED_DOUBLE[] = "clicked,double";
static const char SIG_LOADED_DETAIL[] = "loaded,detail";
static const char SIG_LOAD_DETAIL[] = "load,detail";
static const char SIG_LONGPRESSED[] = "longpressed";
static const char SIG_PRESS[] = "press";
static const char SIG_SCROLL[] = "scroll";
static const char SIG_SCROLL_DRAG_START[] = "scroll,drag,start";
static const char SIG_SCROLL_DRAG_STOP[] = "scroll,drag,stop";
static const char SIG_SCROLL_ANIM_START[] = "scroll,anim,start";
static const char SIG_SCROLL_ANIM_STOP[] = "scroll,anim,stop";
static const char SIG_ZOOM_CHANGE[] = "zoom,change";
static const char SIG_ZOOM_START[] = "zoom,start";
static const char SIG_ZOOM_STOP[] = "zoom,stop";
static const char SIG_DOWNLOADED[] = "downloaded";
static const char SIG_ROUTE_LOAD[] = "route,load";
static const char SIG_ROUTE_LOADED[] = "route,loaded";
static const char SIG_NAME_LOAD[] = "name,load";
static const char SIG_NAME_LOADED[] = "name,loaded";
static const Evas_Smart_Cb_Description _signals[] = {
       {SIG_CHANGED, ""},
       {SIG_CLICKED, ""},
       {SIG_CLICKED_DOUBLE, ""},
       {SIG_LOADED_DETAIL, ""},
       {SIG_LOAD_DETAIL, ""},
       {SIG_LONGPRESSED, ""},
       {SIG_PRESS, ""},
       {SIG_SCROLL, ""},
       {SIG_SCROLL_DRAG_START, ""},
       {SIG_SCROLL_DRAG_STOP, ""},
       {SIG_SCROLL_ANIM_START, ""},
       {SIG_SCROLL_ANIM_STOP, ""},
       {SIG_ZOOM_CHANGE, ""},
       {SIG_ZOOM_START, ""},
       {SIG_ZOOM_STOP, ""},
       {SIG_DOWNLOADED, ""},
       {SIG_ROUTE_LOAD, ""},
       {SIG_ROUTE_LOADED, ""},
       {SIG_NAME_LOAD, ""},
       {SIG_NAME_LOADED, ""},
       {NULL, NULL}
};

static Eina_Bool
module_list_cb(Eina_Module *m, void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(data,EINA_FALSE);
   Widget_Data *wd = data;

   Map_Sources_Tab *s;
   ElmMapModuleSourceFunc source;
   ElmMapModuleZoomMinFunc zoom_min;
   ElmMapModuleZoomMaxFunc zoom_max;
   ElmMapModuleUrlFunc url;
   ElmMapModuleRouteSourceFunc route_source;
   ElmMapModuleRouteUrlFunc route_url;
   ElmMapModuleNameUrlFunc name_url;
   ElmMapModuleGeoIntoCoordFunc geo_into_coord;
   ElmMapModuleCoordIntoGeoFunc coord_into_geo;
   const char *file;

   file = eina_module_file_get(m);
   if (!eina_module_load(m))
     {
        ERR("could not load module \"%s\": %s", file,
            eina_error_msg_get(eina_error_get()));
        return EINA_FALSE;
     }

   source = eina_module_symbol_get(m, "map_module_source_get");
   zoom_min = eina_module_symbol_get(m, "map_module_zoom_min_get");
   zoom_max = eina_module_symbol_get(m, "map_module_zoom_max_get");
   url = eina_module_symbol_get(m, "map_module_url_get");
   route_source = eina_module_symbol_get(m, "map_module_route_source_get");
   route_url = eina_module_symbol_get(m, "map_module_route_url_get");
   name_url = eina_module_symbol_get(m, "map_module_name_url_get");
   geo_into_coord = eina_module_symbol_get(m, "map_module_geo_into_coord");
   coord_into_geo = eina_module_symbol_get(m, "map_module_coord_into_geo");
   if ((!source) || (!zoom_min) || (!zoom_max) || (!url) || (!route_source) ||
       (!route_url) || (!name_url) || (!geo_into_coord) || (!coord_into_geo))
     {
        WRN("could not find map_module_source_get() in module \"%s\": %s",
            file, eina_error_msg_get(eina_error_get()));
        eina_module_unload(m);
        return EINA_FALSE;
     }
   s = ELM_NEW(Map_Sources_Tab);
   s->name = source();
   s->zoom_min = zoom_min();
   s->zoom_max = zoom_max();
   s->url_cb = url;
   s->route_source = route_source();
   s->route_url_cb = route_url;
   s->name_url_cb = name_url;
   s->geo_into_coord = geo_into_coord;
   s->coord_into_geo = coord_into_geo;
   wd->srcs = eina_list_append(wd->srcs, s);

   eina_module_unload(m);
   return EINA_TRUE;
}

static void
source_init(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Map_Sources_Tab *s;
   unsigned int idx;
   Eina_Array *modules = NULL;
   Eina_List *l;

   for (idx = 0; idx < (sizeof(default_map_sources_tab) / sizeof(Map_Sources_Tab)); idx++)
     {
        s = ELM_NEW(Map_Sources_Tab);
        s->name = default_map_sources_tab[idx].name;
        s->zoom_min = default_map_sources_tab[idx].zoom_min;
        s->zoom_max = default_map_sources_tab[idx].zoom_max;
        s->url_cb = default_map_sources_tab[idx].url_cb;
        s->route_source = default_map_sources_tab[idx].route_source;
        s->route_url_cb = default_map_sources_tab[idx].route_url_cb;
        s->name_url_cb = default_map_sources_tab[idx].name_url_cb;
        s->geo_into_coord = default_map_sources_tab[idx].geo_into_coord;
        s->coord_into_geo = default_map_sources_tab[idx].coord_into_geo;
        wd->srcs = eina_list_append(wd->srcs, s);
        if (!idx)
          {
             wd->src = s;
             wd->zoom_min = s->zoom_min;
             wd->zoom_max = s->zoom_max;
          }
     }
   modules = eina_module_list_get(modules, MODULES_PATH, 1, &module_list_cb, wd);
   eina_array_free(modules);

   wd->src_names = calloc((eina_list_count(wd->srcs) + 1), sizeof(char *));
   idx = 0;
   EINA_LIST_FOREACH(wd->srcs, l, s)
     {
        eina_stringshare_replace(&wd->src_names[idx], s->name);
        INF("source : %s", wd->src_names[idx]);
        idx++;
     }
}

static void
_edj_marker_size_get(Widget_Data *wd, Evas_Coord *w, Evas_Coord *h)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);
   EINA_SAFETY_ON_NULL_RETURN(w);
   EINA_SAFETY_ON_NULL_RETURN(h);

   Evas_Object *edj;
   const char *s;

   edj = edje_object_add(evas_object_evas_get(wd->obj));
   _elm_theme_object_set(wd->obj, edj, "map/marker", "radio",
                         elm_widget_style_get(wd->obj));
   s = edje_object_data_get(edj, "size_w");
   if (s) *w = atoi(s);
   else   *w = 0;
   s = edje_object_data_get(edj, "size_h");
   if (s) *h = atoi(s);
   else   *h = 0;
   evas_object_del(edj);
}

static void
_coord_rotate(const Evas_Coord x, const Evas_Coord y, const Evas_Coord cx, const Evas_Coord cy, const double degree, Evas_Coord *xx, Evas_Coord *yy)
{
   EINA_SAFETY_ON_NULL_RETURN(xx);
   EINA_SAFETY_ON_NULL_RETURN(yy);

   double r = (degree * M_PI) / 180.0;
   double tx, ty, ttx, tty;

   tx = x - cx;
   ty = y - cy;

   ttx = tx * cos(r);
   tty = tx * sin(r);
   tx = ttx + (ty * cos(r + M_PI_2));
   ty = tty + (ty * sin(r + M_PI_2));

   *xx = tx + cx;
   *yy = ty + cy;
}

static void
_viewport_size_get(Widget_Data *wd, Evas_Coord *vw, Evas_Coord *vh)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Evas_Coord x, y, w, h;
   evas_object_geometry_get(wd->pan_smart, &x, &y, &w, &h);
   if (vw) *vw = (x * 2) + w;
   if (vh) *vh = (y * 2) + h;
}

static void
_pan_geometry_get(Widget_Data *wd, Evas_Coord *px, Evas_Coord *py)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Evas_Coord x, y, vx, vy, vw, vh;
   elm_smart_scroller_child_pos_get(wd->scr, &x, &y);
   evas_object_geometry_get(wd->pan_smart, &vx, &vy, &vw, &vh);
   x = -x;
   y = -y;
   if (vw > wd->size.w) x += (((vw - wd->size.w) / 2) + vx);
   else x -= vx;
   if (vh > wd->size.h) y += (((vh - wd->size.h) / 2) + vy);
   else y -= vy;
   if (px) *px = x;
   if (py) *py = y;
 }

static void
_obj_rotate(Widget_Data *wd, Evas_Object *obj)
{
   Evas_Coord w, h, ow, oh;
   evas_map_util_points_populate_from_object(wd->map, obj);

   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   evas_object_image_size_get(obj, &w, &h);
   if ((w > ow) || (h > oh))
     {
        evas_map_point_image_uv_set(wd->map, 0, 0, 0);
        evas_map_point_image_uv_set(wd->map, 1, w, 0);
        evas_map_point_image_uv_set(wd->map, 2, w, h);
        evas_map_point_image_uv_set(wd->map, 3, 0, h);
     }
   evas_map_util_rotate(wd->map, wd->rotate.d, wd->rotate.cx, wd->rotate.cy);

   evas_object_map_set(obj, wd->map);
   evas_object_map_enable_set(obj, EINA_TRUE);
}

static void
_obj_place(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   EINA_SAFETY_ON_NULL_RETURN(obj);

   evas_object_move(obj, x, y);
   evas_object_resize(obj, w, h);
   evas_object_show(obj);
}

static void
_bubble_update(Marker_Bubble *bubble, Eina_List *contents)
{
   EINA_SAFETY_ON_NULL_RETURN(bubble);
   EINA_SAFETY_ON_NULL_RETURN(contents);

   Eina_List *l;
   Evas_Object *c;

   elm_box_clear(bubble->bx);
   EINA_LIST_FOREACH(contents, l, c) elm_box_pack_end(bubble->bx, c);
}

static void
_bubble_place(Marker_Bubble *bubble)
{
   EINA_SAFETY_ON_NULL_RETURN(bubble);

   Evas_Coord x, y, w, h;
   Evas_Coord xx, yy, ww, hh;
   const char *s;

   if ((!bubble->obj) || (!bubble->pobj)) return;
   evas_object_geometry_get(bubble->pobj, &x, &y, &w, NULL);

   s = edje_object_data_get(bubble->obj, "size_w");
   if (s) ww = atoi(s);
   else ww = 0;

   edje_object_size_min_calc(bubble->obj, NULL, &hh);
   s = edje_object_data_get(bubble->obj, "size_h");
   if (s) h = atoi(s);
   else h = 0;
   if (hh < h) hh = h;

   xx = x + (w / 2) - (ww / 2);
   yy = y - hh;

   _obj_place(bubble->obj, xx, yy, ww, hh);
   evas_object_raise(bubble->obj);
}

static void
_bubble_sc_hints_changed_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Marker_Bubble *bubble = data;
   EINA_SAFETY_ON_NULL_RETURN(bubble);
   _bubble_place(data);
}

static void
_bubble_mouse_up_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Marker_Bubble *bubble = data;
   EINA_SAFETY_ON_NULL_RETURN(bubble);
   _bubble_place(bubble);
}

static void
_bubble_hide_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Marker_Bubble *bubble = data;
   EINA_SAFETY_ON_NULL_RETURN(bubble);
   evas_object_hide(bubble->obj);
}

static void
_bubble_show_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Marker_Bubble *bubble = data;
   EINA_SAFETY_ON_NULL_RETURN(bubble);
   _bubble_place(bubble);
}

static void
_bubble_move_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Marker_Bubble *bubble = data;
   EINA_SAFETY_ON_NULL_RETURN(bubble);
   _bubble_place(bubble);
}

static void
_bubble_free(Marker_Bubble* bubble)
{
   EINA_SAFETY_ON_NULL_RETURN(bubble);

   evas_object_del(bubble->bx);
   evas_object_del(bubble->sc);
   evas_object_del(bubble->obj);
   free(bubble);
}

static Marker_Bubble*
_bubble_create(Evas_Object *pobj, Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(pobj, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);

   Marker_Bubble *bubble = ELM_NEW(Marker_Bubble);
   EINA_SAFETY_ON_NULL_RETURN_VAL(bubble, NULL);

   bubble->wd = wd;
   bubble->pobj = pobj;
   evas_object_event_callback_add(pobj, EVAS_CALLBACK_HIDE, _bubble_hide_cb,
                                  bubble);
   evas_object_event_callback_add(pobj, EVAS_CALLBACK_SHOW, _bubble_show_cb,
                                  bubble);
   evas_object_event_callback_add(pobj, EVAS_CALLBACK_MOVE, _bubble_move_cb,
                                  bubble);

   bubble->obj = edje_object_add(evas_object_evas_get(pobj));
   _elm_theme_object_set(wd->obj, bubble->obj , "map", "marker_bubble",
                         elm_widget_style_get(wd->obj));
   evas_object_event_callback_add(bubble->obj, EVAS_CALLBACK_MOUSE_UP,
                                  _bubble_mouse_up_cb, bubble);

   bubble->sc = elm_scroller_add(bubble->obj);
   elm_widget_style_set(bubble->sc, "map_bubble");
   elm_scroller_content_min_limit(bubble->sc, EINA_FALSE, EINA_TRUE);
   elm_scroller_policy_set(bubble->sc, ELM_SCROLLER_POLICY_AUTO,
                           ELM_SCROLLER_POLICY_OFF);
   elm_scroller_bounce_set(bubble->sc, _elm_config->thumbscroll_bounce_enable,
                           EINA_FALSE);
   edje_object_part_swallow(bubble->obj, "elm.swallow.content", bubble->sc);
   evas_object_event_callback_add(bubble->sc, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _bubble_sc_hints_changed_cb, bubble);

   bubble->bx = elm_box_add(bubble->sc);
   evas_object_size_hint_align_set(bubble->bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(bubble->bx, EVAS_HINT_EXPAND,
                                    EVAS_HINT_EXPAND);
   elm_box_horizontal_set(bubble->bx, EINA_TRUE);
   elm_object_content_set(bubble->sc, bubble->bx);

   return bubble;
}

static void
_marker_group_update(Marker_Group* group, Elm_Map_Group_Class *clas, Eina_List *markers)
{
   EINA_SAFETY_ON_NULL_RETURN(group);
   EINA_SAFETY_ON_NULL_RETURN(clas);
   EINA_SAFETY_ON_NULL_RETURN(markers);
   Widget_Data *wd = clas->wd;
   EINA_SAFETY_ON_NULL_RETURN(wd);

   char buf[PATH_MAX];
   Eina_List *l;
   Elm_Map_Marker *marker;
   int cnt = 0;
   int sum_x = 0, sum_y = 0;

   EINA_LIST_FOREACH(markers, l, marker)
     {
        sum_x += marker->x;
        sum_y += marker->y;
        cnt++;
     }

   group->x = sum_x / cnt;
   group->y = sum_y / cnt;
   _edj_marker_size_get(wd, &group->w, &group->h);
   group->w *=2;
   group->h *=2;
   group->clas = clas;
   group->markers = markers;

   if (clas->style) elm_layout_theme_set(group->obj, "map/marker", clas->style,
                                         elm_widget_style_get(wd->obj));
   else elm_layout_theme_set(group->obj, "map/marker", "radio",
                             elm_widget_style_get(wd->obj));


   if (clas->func.icon_get)
     {
        Evas_Object *icon = NULL;

        icon = elm_object_part_content_get(group->obj, "elm.icon");
        if (icon) evas_object_del(icon);

        icon = clas->func.icon_get(wd->obj, clas->data);
        elm_object_part_content_set(group->obj, "elm.icon", icon);
     }
   snprintf(buf, sizeof(buf), "%d", cnt);
   edje_object_part_text_set(elm_layout_edje_get(group->obj), "elm.text", buf);

   if (group->bubble)
      {
         Eina_List *l;
         Elm_Map_Marker *marker;
         Eina_List *contents = NULL;

         EINA_LIST_FOREACH(group->markers, l, marker)
           {
              Evas_Object *c = marker->clas->func.get(marker->wd->obj,
                                                      marker, marker->data);
              if (c) contents = eina_list_append(contents, c);
           }
         _bubble_update(group->bubble, contents);
      }
}

static void
_marker_group_bubble_open_cb(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *soure __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Marker_Group *group = data;
   Eina_List *l;
   Elm_Map_Marker *marker;
   Eina_List *contents = NULL;

   if (!group->bubble) group->bubble = _bubble_create(group->obj, group->wd);

   EINA_LIST_FOREACH(group->markers, l, marker)
     {
        if (group->wd->markers_max_num <= eina_list_count(contents)) break;
        Evas_Object *c = marker->clas->func.get(marker->wd->obj,
                                                marker, marker->data);
        if (c) contents = eina_list_append(contents, c);
     }
   _bubble_update(group->bubble, contents);
   _bubble_place(group->bubble);
}

static void
_marker_group_bringin_cb(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *soure __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);

   double lon, lat;
   Marker_Group *group = data;
   elm_map_utils_convert_coord_into_geo(group->wd->obj, group->x, group->y,
                                        group->wd->size.w, &lon, &lat);
   elm_map_geo_region_bring_in(group->wd->obj, lon, lat);
}

static void
_marker_group_free(Marker_Group* group)
{
   EINA_SAFETY_ON_NULL_RETURN(group);

   if (group->bubble) _bubble_free(group->bubble);

   eina_list_free(group->markers);
   evas_object_del(group->obj);

   free(group);
}

static Marker_Group*
_marker_group_create(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);

   Marker_Group *group = ELM_NEW(Marker_Group);

   group->wd = wd;
   group->obj = elm_layout_add(wd->obj);
   evas_object_smart_member_add(group->obj, wd->pan_smart);
   evas_object_stack_above(group->obj, wd->sep_maps_markers);
   elm_layout_theme_set(group->obj, "map/marker", "radio",
                        elm_widget_style_get(wd->obj));
   edje_object_signal_callback_add(elm_layout_edje_get(group->obj),
                                   "open", "elm", _marker_group_bubble_open_cb,
                                   group);
   edje_object_signal_callback_add(elm_layout_edje_get(group->obj),
                                   "bringin", "elm", _marker_group_bringin_cb,
                                   group);
   return group;
}

static void
_marker_bringin_cb(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *soure __UNUSED__)
{
   Elm_Map_Marker *marker = data;
   EINA_SAFETY_ON_NULL_RETURN(marker);
   elm_map_geo_region_bring_in(marker->wd->obj, marker->longitude, marker->latitude);
}

static void
_marker_bubble_open_cb(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *soure __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Elm_Map_Marker *marker = data;

   if (!marker->bubble) marker->bubble = _bubble_create(marker->obj, marker->wd);
   evas_object_smart_changed(marker->wd->pan_smart);
}

static void
_marker_update(Elm_Map_Marker *marker)
{
   EINA_SAFETY_ON_NULL_RETURN(marker);
   Elm_Map_Marker_Class *clas = marker->clas;
   EINA_SAFETY_ON_NULL_RETURN(clas);

   if (clas->style) elm_layout_theme_set(marker->obj, "map/marker", clas->style,
                                         elm_widget_style_get(marker->wd->obj));
   else elm_layout_theme_set(marker->obj, "map/marker", "radio",
                             elm_widget_style_get(marker->wd->obj));

   if (clas->func.icon_get)
     {
        Evas_Object *icon = NULL;

        icon = elm_object_part_content_get(marker->obj, "elm.icon");
        if (icon) evas_object_del(icon);

        icon = clas->func.icon_get(marker->wd->obj, marker, marker->data);
        elm_object_part_content_set(marker->obj, "elm.icon", icon);
     }

   elm_map_utils_convert_geo_into_coord(marker->wd->obj, marker->longitude,
                                        marker->latitude, marker->wd->size.w,
                                        &(marker->x), &(marker->y));

    if (marker->bubble)
      {
         if (marker->content) evas_object_del(marker->content);
         if (marker->clas->func.get)
            marker->content = marker->clas->func.get(marker->wd->obj, marker,
                                                     marker->data);
        if (marker->content)
          {
             Eina_List *contents = NULL;
             contents = eina_list_append(contents, marker->content);
             _bubble_update(marker->bubble, contents);
          }
      }
}



static void
_marker_place(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Eina_List *l;

   Elm_Map_Marker *marker;
   Elm_Map_Group_Class *group_clas;

   Evas_Coord gw, gh;
   Evas_Coord px, py;

   if (wd->paused_markers || (!eina_list_count(wd->markers))) return;

   _pan_geometry_get(wd, &px, &py);

   _edj_marker_size_get(wd, &gw, &gh);
   gw *= 2;
   gh *= 2;

   EINA_LIST_FOREACH(wd->markers, l, marker)
     {
        _marker_update(marker);
        marker->grouped = EINA_FALSE;
        marker->leader = EINA_FALSE;
     }

   EINA_LIST_FOREACH(wd->group_classes, l, group_clas)
     {
        Eina_List *ll;
        EINA_LIST_FOREACH(group_clas->markers, ll, marker)
          {
             Eina_List *lll;
             Elm_Map_Marker *mm;
             Eina_List *markers = NULL;

             if (marker->grouped) continue;
             if (group_clas->zoom_grouped < wd->zoom)
               {
                  marker->grouped = EINA_FALSE;
                  continue;
               }

             EINA_LIST_FOREACH(group_clas->markers, lll, mm)
               {
                  if (marker == mm || mm->grouped) continue;
                  if (ELM_RECTS_INTERSECT(mm->x, mm->y, mm->w, mm->h,
	                                  marker->x, marker->y, gw, gh))
	            {
	               // mm is group follower.
	               mm->leader = EINA_FALSE;
	               mm->grouped = EINA_TRUE;
	               markers = eina_list_append(markers, mm);
	            }
               }
              if (eina_list_count(markers) >= 1)
                {
                   // marker is group leader.
                   marker->leader = EINA_TRUE;
                   marker->grouped = EINA_TRUE;
                   markers = eina_list_append(markers, marker);

                   if (!marker->group) marker->group = _marker_group_create(wd);
                   _marker_group_update(marker->group, group_clas, markers);
                }
          }
     }

   EINA_LIST_FOREACH(wd->markers, l, marker)
     {

        if (marker->grouped ||
           (marker->group_clas &&
            (marker->group_clas->hide ||
             marker->group_clas->zoom_displayed > wd->zoom)))
           evas_object_hide(marker->obj);
        else
          {
             Evas_Coord x, y;
             _coord_rotate(marker->x + px, marker->y + py, wd->rotate.cx,
                           wd->rotate.cy, wd->rotate.d, &x, &y);
             _obj_place(marker->obj, x - (marker->w / 2), y - (marker->h / 2),
                         marker->w, marker->h);
          }
     }

   EINA_LIST_FOREACH(wd->markers, l, marker)
     {
        Marker_Group *group = marker->group;
        if (!group) continue;

        if (!marker->leader || (group->clas->hide) ||
            (group->clas->zoom_displayed > wd->zoom))
           evas_object_hide(group->obj);
        else
          {
             Evas_Coord x, y;
             _coord_rotate(group->x + px, group->y + py, wd->rotate.cx,
                           wd->rotate.cy, wd->rotate.d, &x, &y);
             _obj_place(group->obj, x - (group->w / 2), y - (group->h / 2),
                         group->w, group->h);
          }
     }
}


static void
_grid_item_coord_get(Grid_Item *gi, int *x, int *y, int *w, int *h)
{
   EINA_SAFETY_ON_NULL_RETURN(gi);

   if (x) *x = gi->x * gi->wd->size.tile;
   if (y) *y = gi->y * gi->wd->size.tile;
   if (w) *w = gi->wd->size.tile;
   if (h) *h = gi->wd->size.tile;
}

static Eina_Bool
_grid_item_intersect(Grid_Item *gi)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(gi, EINA_FALSE);

   Evas_Coord px, py;
   Evas_Coord vw, vh;
   Evas_Coord x, y, w, h;

   _pan_geometry_get(gi->wd, &px, &py);
   _viewport_size_get(gi->wd, &vw, &vh);
   _grid_item_coord_get(gi, &x, &y, &w, &h);
   return ELM_RECTS_INTERSECT(x + px, y + py, w, h, 0, 0, vw, vh);
}

static void
_grid_item_update(Grid_Item *gi)
{
   evas_object_image_file_set(gi->img, gi->file, NULL);
   if (!gi->wd->zoom_timer && !gi->wd->scr_timer)
      evas_object_image_smooth_scale_set(gi->img, EINA_TRUE);
   else evas_object_image_smooth_scale_set(gi->img, EINA_FALSE);

   Evas_Load_Error err = evas_object_image_load_error_get(gi->img);
   if (err != EVAS_LOAD_ERROR_NONE)
     {
        ERR("Image loading error (%s): %s", gi->file, evas_load_error_str(err));
        ecore_file_remove(gi->file);
        gi->file_have = EINA_FALSE;
     }
   else
     {
        Evas_Coord px, py;
        Evas_Coord x, y, w, h;

        _pan_geometry_get(gi->wd, &px, &py);
        _grid_item_coord_get(gi, &x, &y, &w, &h);

        _obj_place(gi->img, x + px, y + py, w, h);
        _obj_rotate(gi->wd, gi->img);
        gi->file_have = EINA_TRUE;
     }
}

static void
_grid_item_load(Grid_Item *gi)
{
   EINA_SAFETY_ON_NULL_RETURN(gi);
   if (gi->file_have) _grid_item_update(gi);
   else if (!gi->job)
     {
        gi->wd->download_list = eina_list_remove(gi->wd->download_list, gi);
        gi->wd->download_list = eina_list_append(gi->wd->download_list, gi);
     }
}

static void
_grid_item_unload(Grid_Item *gi)
{
   EINA_SAFETY_ON_NULL_RETURN(gi);
   if (gi->file_have)
     {
        evas_object_hide(gi->img);
        evas_object_image_file_set(gi->img, NULL, NULL);
     }
   else if (gi->job)
     {
        ecore_file_download_abort(gi->job);
        ecore_file_remove(gi->file);
        gi->job = NULL;
        gi->wd->try_num--;
     }
   else gi->wd->download_list = eina_list_remove(gi->wd->download_list, gi);

}

static Grid_Item *
_grid_item_create(Grid *g, Evas_Coord x, Evas_Coord y)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   char buf[PATH_MAX];
   char buf2[PATH_MAX];
   char *source;
   Grid_Item *gi;

   gi = ELM_NEW(Grid_Item);
   gi->wd = g->wd;
   gi->g = g;
   gi->x = x;
   gi->y = y;

   gi->file_have = EINA_FALSE;
   gi->job = NULL;

   gi->img = evas_object_image_add(evas_object_evas_get(g->wd->obj));
   evas_object_image_smooth_scale_set(gi->img, EINA_FALSE);
   evas_object_image_scale_hint_set(gi->img, EVAS_IMAGE_SCALE_HINT_DYNAMIC);
   evas_object_image_filled_set(gi->img, 1);
   evas_object_smart_member_add(gi->img, g->wd->pan_smart);
   evas_object_pass_events_set(gi->img, EINA_TRUE);
   evas_object_stack_below(gi->img, g->wd->sep_maps_markers);

   snprintf(buf, sizeof(buf), CACHE_PATH, g->wd->id, g->zoom, x);
   snprintf(buf2, sizeof(buf2), CACHE_FILE_PATH, buf, y);
   if (!ecore_file_exists(buf)) ecore_file_mkpath(buf);

   eina_stringshare_replace(&gi->file, buf2);
   source = g->wd->src->url_cb(g->wd->obj, x, y, g->zoom);
   if ((!source) || (!strlen(source)))
     {
        eina_stringshare_replace(&gi->source, NULL);
        ERR("Getting source url failed: %s", gi->file);
     }
   else eina_stringshare_replace(&gi->source, source);
   if (source) free(source);
   eina_matrixsparse_data_idx_set(g->grid, y, x, gi);
   return gi;
}

static void
_grid_item_free(Grid_Item *gi)
{
   EINA_SAFETY_ON_NULL_RETURN(gi);

   _grid_item_unload(gi);
   if (gi->g && gi->g->grid) eina_matrixsparse_data_idx_set(gi->g->grid,
                                                            gi->y, gi->x, NULL);
   if (gi->source) eina_stringshare_del(gi->source);
   if (gi->file) eina_stringshare_del(gi->file);
   if (gi->img) evas_object_del(gi->img);
   if (gi->file_have) ecore_file_remove(gi->file);
   free(gi);
}

static void
_downloaded_cb(void *data, const char *file __UNUSED__, int status)
{
   Grid_Item *gi = data;

   if (status == 200)
     {
        DBG("Download success from %s to %s", gi->source, gi->file);
        _grid_item_update(gi);
        gi->wd->finish_num++;
     }
   else
     {
        WRN("Download failed from %s to %s (%d) ", gi->source, gi->file, status);
        ecore_file_remove(gi->file);
        gi->file_have = EINA_FALSE;
     }

   gi->job = NULL;
   gi->wd->download_num--;
   evas_object_smart_callback_call(gi->wd->obj, SIG_DOWNLOADED, NULL);

   if (!gi->wd->download_num)
     {
        edje_object_signal_emit(elm_smart_scroller_edje_object_get(gi->wd->scr),
                                "elm,state,busy,stop", "elm");
        evas_object_smart_callback_call(gi->wd->obj, SIG_LOADED_DETAIL, NULL);
     }
}

static Eina_Bool
_download_job(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(data, ECORE_CALLBACK_CANCEL);
   Widget_Data *wd = data;

   Eina_List *l, *ll;
   Grid_Item *gi;

   if (!eina_list_count(wd->download_list))
     {
        wd->download_idler = NULL;
        return ECORE_CALLBACK_CANCEL;
     }

   EINA_LIST_REVERSE_FOREACH_SAFE(wd->download_list, l, ll, gi)
     {
        if (gi->g->zoom != wd->zoom || !_grid_item_intersect(gi))
          {
             wd->download_list = eina_list_remove(wd->download_list, gi);
             continue;
          }
        if (wd->download_num >= MAX_CONCURRENT_DOWNLOAD)
           return ECORE_CALLBACK_RENEW;

        Eina_Bool ret = ecore_file_download_full(gi->source, gi->file,
                                                 _downloaded_cb, NULL,
                                                 gi, &(gi->job), wd->ua);
        if ((!ret) || (!gi->job))
           ERR("Can't start to download from %s to %s", gi->source, gi->file);
        else
          {
             wd->download_list = eina_list_remove(wd->download_list, gi);

             wd->try_num++;
             wd->download_num++;
             if (wd->download_num == 1)
               edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr),
                                       "elm,state,busy,start", "elm");
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_grid_viewport_get(Grid *g, int *x, int *y, int *w, int *h)
{
   EINA_SAFETY_ON_NULL_RETURN(g);
   int xx, yy, ww, hh;
   Evas_Coord px, py, vw, vh;

   _pan_geometry_get(g->wd, &px, &py);
   _viewport_size_get(g->wd, &vw, &vh);
   if (px > 0) px = 0;
   if (py > 0) py = 0;

   xx = (-px / g->wd->size.tile) - 1;
   if (xx < 0) xx = 0;

   yy = (-py / g->wd->size.tile) - 1;
   if (yy < 0) yy = 0;

   ww = (vw / g->wd->size.tile) + 3;
   if (xx + ww >= g->tw) ww = g->tw - xx;

   hh = (vh / g->wd->size.tile) + 3;
   if (yy + hh >= g->th) hh = g->th - yy;

   if (x) *x = xx;
   if (y) *y = yy;
   if (w) *w = ww;
   if (h) *h = hh;
}

static void
_grid_unload(Grid *g)
{
   EINA_SAFETY_ON_NULL_RETURN(g);
   Eina_Iterator *it;
   Eina_Matrixsparse_Cell *cell;
   Grid_Item *gi;

   it = eina_matrixsparse_iterator_new(g->grid);
   EINA_ITERATOR_FOREACH(it, cell)
     {
        gi = eina_matrixsparse_cell_data_get(cell);
        _grid_item_unload(gi);
     }
   eina_iterator_free(it);
}

static void
_grid_load(Grid *g)
{
   EINA_SAFETY_ON_NULL_RETURN(g);
   int x, y, xx, yy, ww, hh;
   Eina_Iterator *it;
   Eina_Matrixsparse_Cell *cell;
   Grid_Item *gi;

   it = eina_matrixsparse_iterator_new(g->grid);
   EINA_ITERATOR_FOREACH(it, cell)
     {
        gi = eina_matrixsparse_cell_data_get(cell);
        if (!_grid_item_intersect(gi)) _grid_item_unload(gi);
     }
   eina_iterator_free(it);

   _grid_viewport_get(g, &xx, &yy, &ww, &hh);
   for (y = yy; y < yy + hh; y++)
     {
        for (x = xx; x < xx + ww; x++)
          {
             gi = eina_matrixsparse_data_idx_get(g->grid, y, x);
             if (!gi) gi = _grid_item_create(g, x, y);
             _grid_item_load(gi);
          }
     }
}

static void
_grid_place(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);
   Eina_List *l;
   Grid *g;

   EINA_LIST_FOREACH(wd->grids, l, g)
     {
        if (wd->zoom == g->zoom) _grid_load(g);
        else _grid_unload(g);
     }
  if (!wd->download_idler) wd->download_idler = ecore_idler_add(_download_job, wd);
}

static void
_grid_all_create(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);
   EINA_SAFETY_ON_NULL_RETURN(wd->src);

   int zoom;
   for (zoom = wd->src->zoom_min; zoom <= wd->src->zoom_max; zoom++)
     {
        Grid *g;
        int tnum;
        g = ELM_NEW(Grid);
        g->wd = wd;
        g->zoom = zoom;
        tnum =  pow(2.0, g->zoom);
        g->tw = tnum;
        g->th = tnum;
        g->grid = eina_matrixsparse_new(g->th, g->tw, NULL, NULL);
        wd->grids = eina_list_append(wd->grids, g);
     }
}

static void
_grid_all_clear(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Grid *g;
   EINA_LIST_FREE(wd->grids, g)
     {
        Grid_Item *gi;
        Eina_Iterator *it = eina_matrixsparse_iterator_new(g->grid);
        Eina_Matrixsparse_Cell *cell;
        EINA_ITERATOR_FOREACH(it, cell)
          {
             gi = eina_matrixsparse_cell_data_get(cell);
             if (gi) _grid_item_free(gi);
          }
        eina_iterator_free(it);

        eina_matrixsparse_free(g->grid);
        free(g);
     }
   if (!ecore_file_recursive_rm(CACHE_ROOT_PATH))
      ERR("Deletion of %s failed", CACHE_ROOT_PATH);
}

static void
_track_place(Widget_Data *wd)
{
#ifdef ELM_EMAP
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Eina_List *l;
   Evas_Object *route;
   int xmin, xmax, ymin, ymax;
   Evas_Coord px, py, ow, oh;
   px = wd->pan_x;
   py = wd->pan_y;
   _viewport_size_get(wd, &ow, &oh);

   Evas_Coord size = wd->size.w;

   EINA_LIST_FOREACH(wd->track, l, route)
     {
        elm_map_utils_convert_geo_into_coord(wd->obj, elm_route_lon_min_get(route), elm_route_lat_max_get(route), size, &xmin, &ymin);
        elm_map_utils_convert_geo_into_coord(wd->obj, elm_route_lon_max_get(route), elm_route_lat_min_get(route), size, &xmax, &ymax);

        if( !(xmin < px && xmax < px) && !(xmin > px+ow && xmax > px+ow))
        {
           if( !(ymin < py && ymax < py) && !(ymin > py+oh && ymax > py+oh))
           {
              //display the route
              evas_object_move(route, xmin - px, ymin - py);
              evas_object_resize(route, xmax - xmin, ymax - ymin);

              evas_object_raise(route);
              _obj_rotate(wd, route);
              evas_object_show(route);

              continue;
           }
        }
        //the route is not display
        evas_object_hide(route);
     }
#else
   (void) wd;
#endif
}
static void
_route_place(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);
   Eina_List *lr, *lp, *ln;
   Path_Node *n;
   Evas_Object *p;
   Elm_Map_Route *r;
   int nodes;
   int x, y;
   double a;
   Evas_Coord ow, oh;
   Evas_Coord px, py;

   px = wd->pan_x;
   py = wd->pan_y;
   _viewport_size_get(wd, &ow, &oh);

   Evas_Coord size = wd->size.w;

   EINA_LIST_FOREACH(wd->route, lr, r)
     {
        EINA_LIST_FOREACH(r->path, lp, p)
          {
             evas_object_polygon_points_clear(p);
          }

        nodes = eina_list_count(r->nodes);

        EINA_LIST_FOREACH(r->nodes, ln, n)
          {
             if ((!wd->zoom) || ((n->idx) &&
                 ((n->idx % (int)ceil((double)nodes/(double)size*100.0))))) continue;
             if (r->inbound)
               {
                  elm_map_utils_convert_geo_into_coord(wd->obj, n->pos.lon, n->pos.lat, size, &x, &y);
                  if ((x >= px - ow) && (x <= (px + ow*2)) &&
                      (y >= py - oh) && (y <= (py + oh*2)))
                    {
                       x = x - px;
                       y = y - py;

                       p = eina_list_nth(r->path, n->idx);
                       a = (double)(y - r->y) / (double)(x - r->x);
                       if ((abs(a) >= 1) || (r->x == x))
                         {
                            evas_object_polygon_point_add(p, r->x - 3, r->y);
                            evas_object_polygon_point_add(p, r->x + 3, r->y);
                            evas_object_polygon_point_add(p, x + 3, y);
                            evas_object_polygon_point_add(p, x - 3, y);
                         }
                       else
                         {
                            evas_object_polygon_point_add(p, r->x, r->y - 3);
                            evas_object_polygon_point_add(p, r->x, r->y + 3);
                            evas_object_polygon_point_add(p, x, y + 3);
                            evas_object_polygon_point_add(p, x, y - 3);
                         }

                       evas_object_color_set(p, r->color.r, r->color.g, r->color.b, r->color.a);
                       evas_object_raise(p);
                       _obj_rotate(wd, p);
                       evas_object_show(p);
                       r->x = x;
                       r->y = y;
                    }
                  else r->inbound = EINA_FALSE;
               }
             else
               {
                  elm_map_utils_convert_geo_into_coord(wd->obj, n->pos.lon, n->pos.lat, size, &x, &y);
                  if ((x >= px - ow) && (x <= (px + ow*2)) &&
                      (y >= py - oh) && (y <= (py + oh*2)))
                    {
                       r->x = x - px;
                       r->y = y - py;
                       r->inbound = EINA_TRUE;
                    }
                  else r->inbound = EINA_FALSE;
               }
          }
          r->inbound = EINA_FALSE;
     }
}

static void
_delayed_do(Widget_Data *wd)
{
   Delayed_Data *dd;
   dd = eina_list_nth(wd->delayed_jobs, 0);
   if (dd && !dd->wd->zoom_animator)
     {
        dd->func(dd);
        wd->delayed_jobs = eina_list_remove(wd->delayed_jobs, dd);
        free(dd);
     }
}

static void
_smooth_update(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);
   Eina_List *l;
   Grid *g;

   EINA_LIST_FOREACH(wd->grids, l, g)
     {
        Eina_Iterator *it = eina_matrixsparse_iterator_new(g->grid);
        Eina_Matrixsparse_Cell *cell;

        EINA_ITERATOR_FOREACH(it, cell)
          {
             Grid_Item *gi = eina_matrixsparse_cell_data_get(cell);
             if (_grid_item_intersect(gi))
                evas_object_image_smooth_scale_set(gi->img, EINA_TRUE);
          }
        eina_iterator_free(it);
     }
}

static Eina_Bool
_zoom_timeout(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(data, ECORE_CALLBACK_CANCEL);
   Widget_Data *wd = data;
   _smooth_update(wd);
   wd->zoom_timer = NULL;
   evas_object_smart_callback_call(wd->obj, SIG_ZOOM_STOP, NULL);
  return ECORE_CALLBACK_CANCEL;
}

static void
zoom_do(Widget_Data *wd, double zoom)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);
   if (zoom > wd->zoom_max) zoom = wd->zoom_max;
   else if (zoom < wd->zoom_min) zoom = wd->zoom_min;

   Evas_Coord px, py, vw, vh;
   Evas_Coord ow, oh;

   wd->zoom = ROUND(zoom);
   wd->zoom_detail = zoom;
   ow = wd->size.w;
   oh = wd->size.h;
   wd->size.tile = pow(2.0, (zoom - wd->zoom)) * wd->tsize;
   wd->size.w = pow(2.0, wd->zoom) * wd->size.tile;
   wd->size.h = wd->size.w;;

   // Fix to zooming with (viewport center px, py) as the center to prevent
   // from zooming with (0,0) as the cetner. (scroller default behavior)
   _pan_geometry_get(wd, &px, &py);
   _viewport_size_get(wd, &vw, &vh);
   if ((vw > 0) && (vh > 0) && (ow > 0) && (oh > 0))
     {
        Evas_Coord xx, yy;
        double sx, sy;
        if (vw > ow) sx = 0.5;
        else         sx = (double)(-px + (vw / 2)) / ow;
        if (vh > oh) sy = 0.5;
        else         sy = (double)(-py + (vh / 2)) / oh;

        if (sx > 1.0) sx = 1.0;
        if (sy > 1.0) sy = 1.0;

        xx = (sx * wd->size.w) - (vw / 2);
        yy = (sy * wd->size.h) - (vh / 2);
        if (xx < 0) xx = 0;
        else if (xx > (wd->size.w - vw)) xx = wd->size.w - vw;
        if (yy < 0) yy = 0;
        else if (yy > (wd->size.h - vh)) yy = wd->size.h - vh;
        elm_smart_scroller_child_region_show(wd->scr, xx, yy, vw, vh);
     }

   if (wd->zoom_timer) ecore_timer_del(wd->zoom_timer);
   else evas_object_smart_callback_call(wd->obj, SIG_ZOOM_START, NULL);
   wd->zoom_timer = ecore_timer_add(0.25, _zoom_timeout, wd);
   evas_object_smart_callback_call(wd->obj, SIG_ZOOM_CHANGE, NULL);

   evas_object_smart_callback_call(wd->pan_smart, SIG_CHANGED, NULL);
   evas_object_smart_changed(wd->pan_smart);
}

static Eina_Bool
_zoom_anim(void *data)
{
   Widget_Data *wd = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, ECORE_CALLBACK_CANCEL);

   if (wd->ani.cnt <= 0)
     {
        wd->zoom_animator = NULL;
        evas_object_smart_changed(wd->pan_smart);
        return ECORE_CALLBACK_CANCEL;
     }
   else
     {
        wd->ani.zoom += wd->ani.diff;
        wd->ani.cnt--;
        zoom_do(wd, wd->ani.zoom);
        return ECORE_CALLBACK_RENEW;
     }
}

static void
zoom_with_animation(Widget_Data *wd, double zoom, int cnt)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);
   if (cnt == 0) return;

   wd->ani.cnt = cnt;
   wd->ani.zoom = wd->zoom;
   wd->ani.diff = (double)(zoom - wd->zoom) / cnt;
   if (wd->zoom_animator) ecore_animator_del(wd->zoom_animator);
   wd->zoom_animator = ecore_animator_add(_zoom_anim, wd);
}

static void
_sizing_eval(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Evas_Coord maxw = -1, maxh = -1;

   evas_object_size_hint_max_get(wd->scr, &maxw, &maxh);
   evas_object_size_hint_max_set(wd->obj, maxw, maxh);
}

static void
_changed_size_hints(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   _sizing_eval(data);
}

static Eina_Bool
_scr_timeout(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(data, ECORE_CALLBACK_CANCEL);
   Widget_Data *wd = data;
   _smooth_update(wd);
   wd->scr_timer = NULL;
   evas_object_smart_callback_call(wd->obj, SIG_SCROLL_DRAG_STOP, NULL);
   return ECORE_CALLBACK_CANCEL;
}

static void
_scr(void *data, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;

   if (wd->scr_timer) ecore_timer_del(wd->scr_timer);
   else evas_object_smart_callback_call(wd->obj, SIG_SCROLL_DRAG_START, NULL);
   wd->scr_timer = ecore_timer_add(0.25, _scr_timeout, wd);
   evas_object_smart_callback_call(wd->obj, SIG_SCROLL, NULL);
}

static void
_scr_anim_start(void *data, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;
   evas_object_smart_callback_call(wd->obj, SIG_SCROLL_ANIM_START, NULL);
}

static void
_scr_anim_stop(void *data, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;
   evas_object_smart_callback_call(wd->obj, SIG_SCROLL_ANIM_STOP, NULL);
}

static Eina_Bool
_long_press(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(data, ECORE_CALLBACK_CANCEL);
   Widget_Data *wd = data;

   wd->long_timer = NULL;
   evas_object_smart_callback_call(wd->obj, SIG_LONGPRESSED, &wd->ev);
   return ECORE_CALLBACK_CANCEL;
}

static void
_mouse_down(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;
   Evas_Event_Mouse_Down *ev = event_info;

   if (ev->button != 1) return;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) wd->on_hold = EINA_TRUE;
   else wd->on_hold = EINA_FALSE;

   if (ev->flags & EVAS_BUTTON_DOUBLE_CLICK)
     evas_object_smart_callback_call(wd->obj, SIG_CLICKED_DOUBLE, ev);
   else evas_object_smart_callback_call(wd->obj, SIG_PRESS, ev);

   if (wd->long_timer) ecore_timer_del(wd->long_timer);
   wd->ev = *ev;
   wd->long_timer = ecore_timer_add(_elm_config->longpress_timeout, _long_press, wd);
}

static void
_mouse_up(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;

   Evas_Event_Mouse_Up *ev = event_info;
   EINA_SAFETY_ON_NULL_RETURN(ev);

   if (ev->button != 1) return;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) wd->on_hold = EINA_TRUE;
   else wd->on_hold = EINA_FALSE;
   if (wd->long_timer)
     {
        ecore_timer_del(wd->long_timer);
        wd->long_timer = NULL;
     }
   if (!wd->on_hold) evas_object_smart_callback_call(wd->obj, SIG_CLICKED, ev);
   wd->on_hold = EINA_FALSE;
}

static void
_mouse_wheel_cb(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;

   if (!wd->paused)
     {
        Evas_Event_Mouse_Wheel *ev = (Evas_Event_Mouse_Wheel*) event_info;
        zoom_do(wd, wd->zoom_detail - ((double)ev->z / 10));
    }
}

static void
_pan_set(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   Pan *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   if ((x == sd->wd->pan_x) && (y == sd->wd->pan_y)) return;

   sd->wd->pan_x = x;
   sd->wd->pan_y = y;
   evas_object_smart_changed(obj);
}

static void
_pan_get(Evas_Object *obj, Evas_Coord *x, Evas_Coord *y)
{
   Pan *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (x) *x = sd->wd->pan_x;
   if (y) *y = sd->wd->pan_y;
}

static void
_pan_max_get(Evas_Object *obj, Evas_Coord *x, Evas_Coord *y)
{
   Pan *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   Evas_Coord ow, oh;
   evas_object_geometry_get(obj, NULL, NULL, &ow, &oh);
   ow = sd->wd->size.w - ow;
   oh = sd->wd->size.h - oh;
   if (ow < 0) ow = 0;
   if (oh < 0) oh = 0;
   if (x) *x = ow;
   if (y) *y = oh;
}

static void
_pan_min_get(Evas_Object *obj __UNUSED__, Evas_Coord *x, Evas_Coord *y)
{
   if (x) *x = 0;
   if (y) *y = 0;
}

static void
_pan_child_size_get(Evas_Object *obj, Evas_Coord *w, Evas_Coord *h)
{
   Pan *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);
   if (w) *w = sd->wd->size.w;
   if (h) *h = sd->wd->size.h;
}

static void
_pan_add(Evas_Object *obj)
{
   Pan *sd;
   Evas_Object_Smart_Clipped_Data *cd;
   parent_sc.add(obj);
   cd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(cd);
   sd = ELM_NEW(Pan);
   sd->__clipped_data = *cd;
   free(cd);
   evas_object_smart_data_set(obj, sd);
}

static void
_pan_resize(Evas_Object *obj, Evas_Coord w __UNUSED__, Evas_Coord h __UNUSED__)
{
   Pan *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   _sizing_eval(sd->wd);
   elm_map_zoom_mode_set(sd->wd->obj, sd->wd->mode);
   evas_object_smart_changed(obj);
}

static void
_pan_calculate(Evas_Object *obj)
{
   Pan *sd = evas_object_smart_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(sd);

   Evas_Coord w, h;
   evas_object_geometry_get(sd->wd->pan_smart, NULL, NULL, &w, &h);
   if (w <= 0 || h <= 0) return;

   _grid_place(sd->wd);
   _marker_place(sd->wd);
   _route_place(sd->wd);
   _track_place(sd->wd);
   _delayed_do(sd->wd);
}

static void
_pan_move(Evas_Object *obj, Evas_Coord x __UNUSED__, Evas_Coord y __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(obj);
   evas_object_smart_changed(obj);
}

static void
_hold_on(void *data, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;
   elm_smart_scroller_hold_set(wd->scr, 1);
}

static void
_hold_off(void *data, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;
   elm_smart_scroller_hold_set(wd->scr, 0);
}

static void
_freeze_on(void *data, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;
   elm_smart_scroller_freeze_set(wd->scr, 1);
}

static void
_freeze_off(void *data, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Widget_Data *wd = data;
   elm_smart_scroller_freeze_set(wd->scr, 0);
}

static void
_on_focus_hook(void *data __UNUSED__, Evas_Object *obj)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   if (elm_widget_focus_get(obj))
     {
        edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr), "elm,action,focus", "elm");
        evas_object_focus_set(wd->obj, EINA_TRUE);
     }
   else
     {
        edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr), "elm,action,unfocus", "elm");
        evas_object_focus_set(wd->obj, EINA_FALSE);
     }
}

static void
_del_hook(Evas_Object *obj)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Eina_List *l, *ll;
   Evas_Object *p;
   Path_Node *n;
   Path_Waypoint *w;
   Ecore_Event_Handler *h;
   Elm_Map_Route *r;
   Elm_Map_Name *na;
   Evas_Object *route;
   Elm_Map_Marker *marker;
   Elm_Map_Group_Class *group_clas;
   Elm_Map_Marker_Class *clas;
   Delayed_Data *dd;
   int idx = 0;
   Map_Sources_Tab *s;

   EINA_LIST_FOREACH(wd->route, l, r)
     {
        EINA_LIST_FREE(r->path, p)
          {
             evas_object_del(p);
          }

        EINA_LIST_FREE(r->waypoint, w)
          {
             if (w->point) eina_stringshare_del(w->point);
             free(w);
          }

        EINA_LIST_FREE(r->nodes, n)
          {
             if (n->pos.address) eina_stringshare_del(n->pos.address);
             free(n);
          }

        EINA_LIST_FREE(r->handlers, h)
          {
             ecore_event_handler_del(h);
          }

        if (r->con_url) ecore_con_url_free(r->con_url);
        if (r->info.nodes) eina_stringshare_del(r->info.nodes);
        if (r->info.waypoints) eina_stringshare_del(r->info.waypoints);
     }

   EINA_LIST_FREE(wd->names, na)
     {
        if (na->address) free(na->address);
        if (na->handler) ecore_event_handler_del(na->handler);
        if (na->ud.fname)
          {
             ecore_file_remove(na->ud.fname);
             free(na->ud.fname);
             na->ud.fname = NULL;
          }
     }

   EINA_LIST_FREE(wd->track, route)
     {
        evas_object_del(route);
     }


   EINA_LIST_FOREACH_SAFE(wd->markers, l, ll, marker)
      elm_map_marker_remove(marker);
   eina_list_free(wd->markers);

   EINA_LIST_FREE(wd->group_classes, group_clas)
     {
        eina_list_free(group_clas->markers);
        if (group_clas->style) eina_stringshare_del(group_clas->style);
        free(group_clas);
     }

   EINA_LIST_FREE(wd->marker_classes, clas)
     {
        if (clas->style) eina_stringshare_del(clas->style);
        free(clas);
     }

   if (wd->scr_timer) ecore_timer_del(wd->scr_timer);
   if (wd->long_timer) ecore_timer_del(wd->long_timer);

   if (wd->delayed_jobs) EINA_LIST_FREE(wd->delayed_jobs, dd) free(dd);

   if (wd->user_agent) eina_stringshare_del(wd->user_agent);
   if (wd->ua) eina_hash_free(wd->ua);
   if (wd->download_idler) ecore_idler_del(wd->download_idler);
   eina_list_free(wd->download_list);

   if (wd->zoom_timer) ecore_timer_del(wd->zoom_timer);
   if (wd->zoom_animator) ecore_animator_del(wd->zoom_animator);

   _grid_all_clear(wd);

   for (idx = 0; wd->src_names[idx]; idx++)
      eina_stringshare_del(wd->src_names[idx]);

   EINA_LIST_FREE(wd->srcs, s) free(s);

   if (wd->map) evas_map_free(wd->map);

   free(wd);
}

static void
_del_pre_hook(Evas_Object *obj)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);
}

static void
_theme_hook(Evas_Object *obj)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   elm_smart_scroller_object_theme_set(obj, wd->scr, "map", "base", elm_widget_style_get(obj));
   _sizing_eval(wd);
}

static Eina_Bool
_event_hook(Evas_Object *obj, Evas_Object *src __UNUSED__, Evas_Callback_Type type, void *event_info)
{
   ELM_CHECK_WIDTYPE(obj, widtype) EINA_FALSE;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EINA_FALSE);

   Evas_Coord x, y;
   Evas_Coord vh;
   Evas_Coord step_x, step_y, page_x, page_y;

   if (type != EVAS_CALLBACK_KEY_DOWN) return EINA_FALSE;
   Evas_Event_Key_Down *ev = event_info;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return EINA_FALSE;

   elm_smart_scroller_child_pos_get(wd->scr, &x, &y);
   elm_smart_scroller_step_size_get(wd->scr, &step_x, &step_y);
   elm_smart_scroller_page_size_get(wd->scr, &page_x, &page_y);
   _viewport_size_get(wd, NULL, &vh);

   if ((!strcmp(ev->keyname, "Left")) || (!strcmp(ev->keyname, "KP_Left")))
     {
        x -= step_x;
     }
   else if ((!strcmp(ev->keyname, "Right")) || (!strcmp(ev->keyname, "KP_Right")))
     {
        x += step_x;
     }
   else if ((!strcmp(ev->keyname, "Up"))  || (!strcmp(ev->keyname, "KP_Up")))
     {
        y -= step_y;
     }
   else if ((!strcmp(ev->keyname, "Down")) || (!strcmp(ev->keyname, "KP_Down")))
     {
        y += step_y;
     }
   else if ((!strcmp(ev->keyname, "Prior")) || (!strcmp(ev->keyname, "KP_Prior")))
     {
        if (page_y < 0)
          y -= -(page_y * vh) / 100;
        else
          y -= page_y;
     }
   else if ((!strcmp(ev->keyname, "Next")) || (!strcmp(ev->keyname, "KP_Next")))
     {
        if (page_y < 0)
          y += -(page_y * vh) / 100;
        else
          y += page_y;
     }
   else if (!strcmp(ev->keyname, "KP_Add"))
     {
        zoom_with_animation(wd, wd->zoom + 1, 10);
        return EINA_TRUE;
     }
   else if (!strcmp(ev->keyname, "KP_Subtract"))
     {
        zoom_with_animation(wd, wd->zoom - 1, 10);
        return EINA_TRUE;
     }
   else return EINA_FALSE;

   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   elm_smart_scroller_child_pos_set(wd->scr, x, y);

   return EINA_TRUE;
}

static Eina_Bool
cb_dump_name_attrs(void *data, const char *key, const char *value)
{
   Name_Dump *dump = (Name_Dump*)data;
   if (!dump) return EINA_FALSE;

   if (!strncmp(key, NOMINATIM_ATTR_LON, sizeof(NOMINATIM_ATTR_LON))) dump->lon = atof(value);
   else if (!strncmp(key, NOMINATIM_ATTR_LAT, sizeof(NOMINATIM_ATTR_LAT))) dump->lat = atof(value);

   return EINA_TRUE;
}

static Eina_Bool
cb_route_dump(void *data, Eina_Simple_XML_Type type, const char *value, unsigned offset __UNUSED__, unsigned length)
{
   Route_Dump *dump = data;
   if (!dump) return EINA_FALSE;

   switch (type)
     {
      case EINA_SIMPLE_XML_OPEN:
      case EINA_SIMPLE_XML_OPEN_EMPTY:
        {
           const char *attrs;

           attrs = eina_simple_xml_tag_attributes_find(value, length);
           if (!attrs)
             {
                if (!strncmp(value, YOURS_DISTANCE, length)) dump->id = ROUTE_XML_DISTANCE;
                else if (!strncmp(value, YOURS_DESCRIPTION, length)) dump->id = ROUTE_XML_DESCRIPTION;
                else if (!strncmp(value, YOURS_COORDINATES, length)) dump->id = ROUTE_XML_COORDINATES;
                else dump->id = ROUTE_XML_NONE;
             }
         }
        break;
      case EINA_SIMPLE_XML_DATA:
        {
           char *buf = malloc(length);
           if (!buf) return EINA_FALSE;
           snprintf(buf, length, "%s", value);
           if (dump->id == ROUTE_XML_DISTANCE) dump->distance = atof(buf);
           else if (!(dump->description) && (dump->id == ROUTE_XML_DESCRIPTION)) dump->description = strdup(buf);
           else if (dump->id == ROUTE_XML_COORDINATES) dump->coordinates = strdup(buf);
           free(buf);
        }
        break;
      default:
        break;
     }

   return EINA_TRUE;
}

static Eina_Bool
cb_name_dump(void *data, Eina_Simple_XML_Type type, const char *value, unsigned offset __UNUSED__, unsigned length)
{
   Name_Dump *dump = data;
   if (!dump) return EINA_FALSE;

   switch (type)
     {
      case EINA_SIMPLE_XML_OPEN:
      case EINA_SIMPLE_XML_OPEN_EMPTY:
        {
           const char *attrs;
           attrs = eina_simple_xml_tag_attributes_find(value, length);
           if (attrs)
             {
                if (!strncmp(value, NOMINATIM_RESULT, sizeof(NOMINATIM_RESULT) - 1)) dump->id = NAME_XML_NAME;
                else dump->id = NAME_XML_NONE;

                eina_simple_xml_attributes_parse
                  (attrs, length - (attrs - value), cb_dump_name_attrs, dump);
             }
        }
        break;
      case EINA_SIMPLE_XML_DATA:
        {
           char *buf = malloc(length + 1);
           if (!buf) return EINA_FALSE;
           snprintf(buf, length + 1, "%s", value);
           if (dump->id == NAME_XML_NAME) dump->address = strdup(buf);
           free(buf);
        }
        break;
      default:
        break;
     }

   return EINA_TRUE;
}

static void
_parse_kml(void *data)
{
   Elm_Map_Route *r = (Elm_Map_Route*)data;
   if (!r || !r->ud.fname) return;

   FILE *f;
   char **str;
   unsigned int ele, idx;
   double lon, lat;
   Evas_Object *path;

   Route_Dump dump = {0, r->ud.fname, 0.0, NULL, NULL};

   f = fopen(r->ud.fname, "rb");
   if (f)
     {
        long sz;

        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        if (sz > 0)
          {
             char *buf;

             fseek(f, 0, SEEK_SET);
             buf = malloc(sz);
             if (buf)
               {
                  if (fread(buf, 1, sz, f))
                    {
                       eina_simple_xml_parse(buf, sz, EINA_TRUE, cb_route_dump, &dump);
                       free(buf);
                    }
               }
          }
        fclose(f);

        if (dump.distance) r->info.distance = dump.distance;
        if (dump.description)
          {
             eina_stringshare_replace(&r->info.waypoints, dump.description);
             str = eina_str_split_full(dump.description, "\n", 0, &ele);
             r->info.waypoint_count = ele;
             for (idx = 0; idx < ele; idx++)
               {
                  Path_Waypoint *wp = ELM_NEW(Path_Waypoint);
                  if (wp)
                    {
                       wp->wd = r->wd;
                       wp->point = eina_stringshare_add(str[idx]);
                       DBG("%s", str[idx]);
                       r->waypoint = eina_list_append(r->waypoint, wp);
                    }
               }
             if (str && str[0])
               {
                  free(str[0]);
                  free(str);
               }
          }
        else WRN("description is not found !");

        if (dump.coordinates)
          {
             eina_stringshare_replace(&r->info.nodes, dump.coordinates);
             str = eina_str_split_full(dump.coordinates, "\n", 0, &ele);
             r->info.node_count = ele;
             for (idx = 0; idx < ele; idx++)
               {
                  sscanf(str[idx], "%lf,%lf", &lon, &lat);
                  Path_Node *n = ELM_NEW(Path_Node);
                  if (n)
                    {
                       n->wd = r->wd;
                       n->pos.lon = lon;
                       n->pos.lat = lat;
                       n->idx = idx;
                       DBG("%lf:%lf", lon, lat);
                       n->pos.address = NULL;
                       r->nodes = eina_list_append(r->nodes, n);

                       path = evas_object_polygon_add(evas_object_evas_get(r->wd->obj));
                       evas_object_smart_member_add(path, r->wd->pan_smart);
                       r->path = eina_list_append(r->path, path);
                    }
               }
             if (str && str[0])
               {
                  free(str[0]);
                  free(str);
               }
          }
     }
}

static void
_parse_name(void *data)
{
   Elm_Map_Name *n = (Elm_Map_Name*)data;
   if (!n || !n->ud.fname) return;

   FILE *f;

   Name_Dump dump = {0, NULL, 0.0, 0.0};

   f = fopen(n->ud.fname, "rb");
   if (f)
     {
        long sz;

        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        if (sz > 0)
          {
             char *buf;

             fseek(f, 0, SEEK_SET);
             buf = malloc(sz);
             if (buf)
               {
                  if (fread(buf, 1, sz, f))
                    {
                       eina_simple_xml_parse(buf, sz, EINA_TRUE, cb_name_dump, &dump);
                       free(buf);
                    }
               }
          }
        fclose(f);

        if (dump.address)
          {
             INF("[%lf : %lf] ADDRESS : %s", n->lon, n->lat, dump.address);
             n->address = strdup(dump.address);
          }
        n->lon = dump.lon;
        n->lat = dump.lat;
     }
}

Grid *_get_current_grid(Widget_Data *wd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);
   Eina_List *l;
   Grid *g = NULL, *ret = NULL;
   EINA_LIST_FOREACH(wd->grids, l, g)
     {
        if (wd->zoom == g->zoom)
          {
             ret = g;
             break;
          }
     }
   return ret;
}

static Eina_Bool
_route_complete_cb(void *data, int ev_type __UNUSED__, void *event)
{
   Ecore_Con_Event_Url_Complete *ev = event;
   Elm_Map_Route *r = (Elm_Map_Route*)data;
   Widget_Data *wd = r->wd;

   if ((!r) || (!ev)) return EINA_TRUE;
   Elm_Map_Route *rr = ecore_con_url_data_get(r->con_url);
   ecore_con_url_data_set(r->con_url, NULL);
   if (r!=rr) return EINA_TRUE;

   if (r->ud.fd) fclose(r->ud.fd);
   _parse_kml(r);

   _route_place(wd);

   edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr),
                           "elm,state,busy,stop", "elm");
   evas_object_smart_callback_call(wd->obj, SIG_ROUTE_LOADED, NULL);
   return EINA_TRUE;
}

static Eina_Bool
_name_complete_cb(void *data, int ev_type __UNUSED__, void *event)
{
   Ecore_Con_Event_Url_Complete *ev = event;
   Elm_Map_Name *n = (Elm_Map_Name*)data;
   Widget_Data *wd = n->wd;

   if ((!n) || (!ev)) return EINA_TRUE;
   Elm_Map_Name *nn = ecore_con_url_data_get(n->con_url);
   ecore_con_url_data_set(n->con_url, NULL);
   if (n!=nn) return EINA_TRUE;

   if (n->ud.fd) fclose(n->ud.fd);
   _parse_name(n);

   edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr),
                           "elm,state,busy,stop", "elm");
   evas_object_smart_callback_call(wd->obj, SIG_NAME_LOADED, NULL);
   return EINA_TRUE;
}

static Elm_Map_Name *
_utils_convert_name(const Evas_Object *obj, int method, char *address, double lon, double lat)
{
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);
   char buf[PATH_MAX];
   char *source;
   int fd;

   if ((!wd) || (!wd->src)) return NULL;
   Elm_Map_Name *name = ELM_NEW(Elm_Map_Name);

   snprintf(buf, sizeof(buf), DEST_NAME_XML_FILE);
   fd = mkstemp(buf);
   if (fd < 0)
     {
        free(name);
        return NULL;
     }

   name->con_url = ecore_con_url_new(NULL);
   name->ud.fname = strdup(buf);
   INF("xml file : %s", name->ud.fname);

   name->ud.fd = fdopen(fd, "w+");
   if ((!name->con_url) || (!name->ud.fd))
     {
        ecore_con_url_free(name->con_url);
        free(name);
        return NULL;
     }

   name->wd = wd;
   name->handler = ecore_event_handler_add (ECORE_CON_EVENT_URL_COMPLETE, _name_complete_cb, name);
   name->method = method;
   if (method == ELM_MAP_NAME_METHOD_SEARCH) name->address = strdup(address);
   else if (method == ELM_MAP_NAME_METHOD_REVERSE) name->address = NULL;
   name->lon = lon;
   name->lat = lat;

   source = wd->src->name_url_cb(wd->obj, method, address, lon, lat);
   INF("name url = %s", source);

   wd->names = eina_list_append(wd->names, name);
   ecore_con_url_url_set(name->con_url, source);
   ecore_con_url_fd_set(name->con_url, fileno(name->ud.fd));
   ecore_con_url_data_set(name->con_url, name);

   edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr),
                           "elm,state,busy,start", "elm");
   evas_object_smart_callback_call(wd->obj, SIG_NAME_LOAD, NULL);
   ecore_con_url_get(name->con_url);
   if (source) free(source);

   return name;

}

static Evas_Event_Flags
_pinch_zoom_start_cb(void *data, void *event_info __UNUSED__)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(data, EVAS_EVENT_FLAG_NONE);
   Widget_Data *wd = data;

   wd->pinch_zoom = wd->zoom_detail;
   return EVAS_EVENT_FLAG_NONE;
}

static Evas_Event_Flags
_pinch_zoom_cb(void *data, void *event_info)
{
   Widget_Data *wd = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EVAS_EVENT_FLAG_NONE);

   if (!wd->paused)
     {
        Elm_Gesture_Zoom_Info *ei = event_info;
        zoom_do(wd, wd->pinch_zoom + ei->zoom - 1);
     }
   return EVAS_EVENT_FLAG_NONE;
}

static Evas_Event_Flags
_pinch_rotate_cb(void *data, void *event_info)
{
   Widget_Data *wd = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EVAS_EVENT_FLAG_NONE);

   if (!wd->paused)
     {
        int x, y, w, h;
        Elm_Gesture_Rotate_Info *ei = event_info;
        evas_object_geometry_get(wd->obj, &x, &y, &w, &h);

        wd->rotate.d = wd->rotate.a + ei->angle - ei->base_angle;
        wd->rotate.cx = x + ((double)w * 0.5);
        wd->rotate.cy = y + ((double)h * 0.5);

        evas_object_smart_changed(wd->pan_smart);
     }
   return EVAS_EVENT_FLAG_NONE;
}

static Evas_Event_Flags
_pinch_rotate_end_cb(void *data, void *event_info __UNUSED__)
{
   Widget_Data *wd = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EVAS_EVENT_FLAG_NONE);

   wd->rotate.a = wd->rotate.d;

   return EVAS_EVENT_FLAG_NONE;
}

static void
_zoom_mode_set(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Delayed_Data *dd = data;

   dd->wd->mode = dd->mode;
   if (dd->mode != ELM_MAP_ZOOM_MODE_MANUAL)
     {
        Evas_Coord w, h;
        Evas_Coord vw, vh;

        double zoom;
        double diff;

        w = dd->wd->size.w;
        h = dd->wd->size.h;
        zoom = dd->wd->zoom_detail;
        _viewport_size_get(dd->wd, &vw, &vh);

        if (dd->mode == ELM_MAP_ZOOM_MODE_AUTO_FIT)
          {
             if ((w < vw) && (h < vh))
               {
                  diff = 0.01;
                  while ((w < vw) && (h < vh))
                  {
                     zoom += diff;
                     w = pow(2.0, zoom) * dd->wd->tsize;
                     h = pow(2.0, zoom) * dd->wd->tsize;
                  }
               }
             else
               {
                  diff = -0.01;
                  while ((w > vw) || (h > vh))
                  {
                     zoom += diff;
                     w = pow(2.0, zoom) * dd->wd->tsize;
                     h = pow(2.0, zoom) * dd->wd->tsize;
                  }
               }

          }
        else if (dd->mode == ELM_MAP_ZOOM_MODE_AUTO_FILL)
          {
             if ((w < vw) || (h < vh))
               {
                  diff = 0.01;
                  while ((w < vw) || (h < vh))
                  {
                     zoom += diff;
                     w = pow(2.0, zoom) * dd->wd->tsize;
                     h = pow(2.0, zoom) * dd->wd->tsize;
                  }
               }
             else
               {
                  diff = -0.01;
                  while ((w > vw) && (h > vh))
                  {
                     zoom += diff;
                     w = pow(2.0, zoom) * dd->wd->tsize;
                     h = pow(2.0, zoom) * dd->wd->tsize;
                  }
               }
          }
       zoom_with_animation(dd->wd, zoom, 10);
     }
}

static void
_zoom_set(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Delayed_Data *dd = data;

   if (dd->wd->paused) zoom_do(dd->wd, dd->zoom);
   else zoom_with_animation(dd->wd, dd->zoom, 10);
   evas_object_smart_changed(dd->wd->pan_smart);
}

static void
_region_bring_in(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Delayed_Data *dd = data;
   int x, y, w, h;

   elm_map_utils_convert_geo_into_coord(dd->wd->obj, dd->lon, dd->lat,
                                        dd->wd->size.w, &x, &y);
   _viewport_size_get(dd->wd, &w, &h);
   x = x - (w / 2);
   y = y - (h / 2);
   elm_smart_scroller_region_bring_in(dd->wd->scr, x, y, w, h);
   evas_object_smart_changed(dd->wd->pan_smart);
}

static void
_region_show(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Delayed_Data *dd = data;
   int x, y, w, h;

   elm_map_utils_convert_geo_into_coord(dd->wd->obj, dd->lon, dd->lat,
                                        dd->wd->size.w, &x, &y);
   _viewport_size_get(dd->wd, &w, &h);
   x = x - (w / 2);
   y = y - (h / 2);
   elm_smart_scroller_child_region_show(dd->wd->scr, x, y, w, h);
   evas_object_smart_changed(dd->wd->pan_smart);
}

static void
_marker_list_show(void *data)
{
   EINA_SAFETY_ON_NULL_RETURN(data);
   Delayed_Data *dd = data;
   int zoom;
   double max_lon = -180, min_lon = 180;
   double max_lat = -90, min_lat = 90;
   Evas_Coord vw, vh;
   Elm_Map_Marker *marker;

   EINA_LIST_FREE(dd->markers, marker)
     {
        if (marker->longitude > max_lon) max_lon = marker->longitude;
        if (marker->longitude < min_lon) min_lon = marker->longitude;
        if (marker->latitude > max_lat) max_lat = marker->latitude;
        if (marker->latitude < min_lat) min_lat = marker->latitude;
     }
   dd->lon = (max_lon + min_lon) / 2;
   dd->lat = (max_lat + min_lat) / 2;

   zoom = dd->wd->src->zoom_min;
   _viewport_size_get(dd->wd, &vw, &vh);
   while (zoom <= dd->wd->src->zoom_max)
     {
        Evas_Coord size, max_x, max_y, min_x, min_y;
        size = pow(2.0, zoom) * dd->wd->tsize;
        elm_map_utils_convert_geo_into_coord(dd->wd->obj, min_lon, max_lat, size, &min_x, &max_y);
        elm_map_utils_convert_geo_into_coord(dd->wd->obj, max_lon, min_lat, size, &max_x, &min_y);
        if ((max_x - min_x) > vw || (max_y - min_y) > vh) break;
        zoom++;
     }
   zoom--;

   zoom_do(dd->wd, zoom);
   _region_show(dd);
   evas_object_smart_changed(dd->wd->pan_smart);
}

static char *
_mapnik_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom)
{
   char buf[PATH_MAX];
   // ((x+y+zoom)%3)+'a' is requesting map images from distributed tile servers (eg., a, b, c)
   snprintf(buf, sizeof(buf), "http://%c.tile.openstreetmap.org/%d/%d/%d.png",
            ((x + y + zoom) % 3) + 'a', zoom, x, y);
   return strdup(buf);
}

static char *
_osmarender_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "http://%c.tah.openstreetmap.org/Tiles/tile/%d/%d/%d.png",
            ((x + y + zoom) % 3) + 'a', zoom, x, y);
   return strdup(buf);
}

static char *
_cyclemap_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "http://%c.tile.opencyclemap.org/cycle/%d/%d/%d.png",
            (( x + y + zoom) % 3) + 'a', zoom, x, y);
   return strdup(buf);
}

static char *
_mapquest_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "http://otile%d.mqcdn.com/tiles/1.0.0/osm/%d/%d/%d.png",
            ((x + y + zoom) % 4) + 1, zoom, x, y);
   return strdup(buf);
}

static char *
_mapquest_aerial_url_cb(Evas_Object *obj __UNUSED__, int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf), "http://oatile%d.mqcdn.com/naip/%d/%d/%d.png",
           ((x + y + zoom) % 4) + 1, zoom, x, y);
   return strdup(buf);
}

static char *_yours_url_cb(Evas_Object *obj __UNUSED__, char *type_name, int method, double flon, double flat, double tlon, double tlat)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "%s?flat=%lf&flon=%lf&tlat=%lf&tlon=%lf&v=%s&fast=%d&instructions=1",
            ROUTE_YOURS_URL, flat, flon, tlat, tlon, type_name, method);

   return strdup(buf);
}

// TODO: fix monav api
/*
static char *_monav_url_cb(Evas_Object *obj __UNUSED__, char *type_name, int method, double flon, double flat, double tlon, double tlat)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "%s?flat=%f&flon=%f&tlat=%f&tlon=%f&v=%s&fast=%d&instructions=1",
            ROUTE_MONAV_URL, flat, flon, tlat, tlon, type_name, method);

   return strdup(buf);
}
*/

// TODO: fix ors api
/*
static char *_ors_url_cb(Evas_Object *obj __UNUSED__, char *type_name, int method, double flon, double flat, double tlon, double tlat)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "%s?flat=%f&flon=%f&tlat=%f&tlon=%f&v=%s&fast=%d&instructions=1",
            ROUTE_ORS_URL, flat, flon, tlat, tlon, type_name, method);

   return strdup(buf);
}
*/

static char *
_nominatim_url_cb(Evas_Object *obj, int method, char *name, double lon, double lat)
{
   ELM_CHECK_WIDTYPE(obj, widtype) strdup("");
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, strdup(""));

   char **str;
   unsigned int ele, idx;
   char search_url[PATH_MAX];
   char buf[PATH_MAX];

   if (method == ELM_MAP_NAME_METHOD_SEARCH)
     {
        search_url[0] = '\0';
        str = eina_str_split_full(name, " ", 0, &ele);
        for (idx = 0; idx < ele; idx++)
          {
             eina_strlcat(search_url, str[idx], sizeof(search_url));
             if (!(idx == (ele-1)))
                eina_strlcat(search_url, "+", sizeof(search_url));
          }
        snprintf(buf, sizeof(buf),
                 "%s/search?q=%s&format=xml&polygon=0&addressdetails=0",
                 NAME_NOMINATIM_URL, search_url);

        if (str && str[0])
          {
             free(str[0]);
             free(str);
          }
     }
   else if (method == ELM_MAP_NAME_METHOD_REVERSE)
      snprintf(buf, sizeof(buf),
               "%s/reverse?format=xml&lat=%lf&lon=%lf&zoom=%d&addressdetails=0",
               NAME_NOMINATIM_URL, lat, lon, (int)wd->zoom);
   else strcpy(buf, "");

   return strdup(buf);
}

#endif

EAPI Evas_Object *
elm_map_add(Evas_Object *parent)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   Evas *e;
   Widget_Data *wd;
   Evas_Object *obj;
   Evas_Coord minw, minh;

   ELM_WIDGET_STANDARD_SETUP(wd, Widget_Data, parent, e, obj, NULL);
   ELM_SET_WIDTYPE(widtype, "map");
   elm_widget_type_set(obj, "map");
   elm_widget_sub_object_add(parent, obj);
   elm_widget_data_set(obj, wd);
   elm_widget_can_focus_set(obj, EINA_TRUE);
   elm_widget_on_focus_hook_set(obj, _on_focus_hook, NULL);
   elm_widget_del_hook_set(obj, _del_hook);
   elm_widget_del_pre_hook_set(obj, _del_pre_hook);
   elm_widget_theme_hook_set(obj, _theme_hook);
   elm_widget_event_hook_set(obj, _event_hook);
   evas_object_smart_callback_add(obj, "scroll-hold-on", _hold_on, wd);
   evas_object_smart_callback_add(obj, "scroll-hold-off", _hold_off, wd);
   evas_object_smart_callback_add(obj, "scroll-freeze-on", _freeze_on, wd);
   evas_object_smart_callback_add(obj, "scroll-freeze-off", _freeze_off, wd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_DOWN,
                                  _mouse_down, wd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_UP,
                                  _mouse_up, wd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _mouse_wheel_cb,wd);
   wd->obj = obj;

   wd->scr = elm_smart_scroller_add(e);
   elm_widget_sub_object_add(obj, wd->scr);
   elm_smart_scroller_widget_set(wd->scr, obj);
   elm_smart_scroller_object_theme_set(obj, wd->scr, "map", "base", "default");
   elm_widget_resize_object_set(obj, wd->scr);
   elm_smart_scroller_wheel_disabled_set(wd->scr, EINA_TRUE);
   elm_smart_scroller_bounce_allow_set(wd->scr,
                                       _elm_config->thumbscroll_bounce_enable,
                                       _elm_config->thumbscroll_bounce_enable);
   evas_object_event_callback_add(wd->scr, EVAS_CALLBACK_CHANGED_SIZE_HINTS,
                                  _changed_size_hints, wd);
   evas_object_smart_callback_add(wd->scr, "scroll", _scr, wd);
   evas_object_smart_callback_add(wd->scr, "drag", _scr, wd);
   evas_object_smart_callback_add(wd->scr, "animate,start", _scr_anim_start, wd);
   evas_object_smart_callback_add(wd->scr, "animate,stop", _scr_anim_stop, wd);

   if (!smart)
     {
        evas_object_smart_clipped_smart_set(&parent_sc);
        sc = parent_sc;
        sc.name = "elm_map_pan";
        sc.version = EVAS_SMART_CLASS_VERSION;
        sc.add = _pan_add;
        sc.resize = _pan_resize;
        sc.move = _pan_move;
        sc.calculate = _pan_calculate;
        smart = evas_smart_class_new(&sc);
     }
   if (smart)
     {
        Pan *pan;
        wd->pan_smart = evas_object_smart_add(e, smart);
        pan = evas_object_smart_data_get(wd->pan_smart);
        pan->wd = wd;
     }
   elm_widget_sub_object_add(obj, wd->pan_smart);

   elm_smart_scroller_extern_pan_set(wd->scr, wd->pan_smart,
                                     _pan_set, _pan_get, _pan_max_get,
                                     _pan_min_get, _pan_child_size_get);
   edje_object_size_min_calc(elm_smart_scroller_edje_object_get(wd->scr),
                             &minw, &minh);
   evas_object_size_hint_min_set(obj, minw, minh);

   wd->ges = elm_gesture_layer_add(obj);
   if (!wd->ges) ERR("elm_gesture_layer_add() failed");
   elm_gesture_layer_attach(wd->ges, obj);
   elm_gesture_layer_cb_set(wd->ges, ELM_GESTURE_ZOOM, ELM_GESTURE_STATE_START,
                            _pinch_zoom_start_cb, wd);
   elm_gesture_layer_cb_set(wd->ges, ELM_GESTURE_ZOOM, ELM_GESTURE_STATE_MOVE,
                            _pinch_zoom_cb, wd);
   elm_gesture_layer_cb_set(wd->ges, ELM_GESTURE_ROTATE, ELM_GESTURE_STATE_MOVE,
                            _pinch_rotate_cb, wd);
   elm_gesture_layer_cb_set(wd->ges, ELM_GESTURE_ROTATE, ELM_GESTURE_STATE_END,
                            _pinch_rotate_end_cb, wd);
   elm_gesture_layer_cb_set(wd->ges, ELM_GESTURE_ROTATE, ELM_GESTURE_STATE_ABORT,
                            _pinch_rotate_end_cb, wd);

   wd->sep_maps_markers = evas_object_rectangle_add(evas_object_evas_get(obj));
   elm_widget_sub_object_add(obj, wd->sep_maps_markers);
   evas_object_smart_member_add(wd->sep_maps_markers, wd->pan_smart);

   wd->map = evas_map_new(EVAS_MAP_POINT);

   source_init(wd);
   wd->tsize = DEFAULT_TILE_SIZE; // FIXME: It should be hard-coded ? or can get from provider?

   wd->id = ((int)getpid() << 16) | idnum;
   idnum++;
   _grid_all_create(wd);

   zoom_do(wd, 0);

   wd->mode = ELM_MAP_ZOOM_MODE_MANUAL;
   wd->markers_max_num = MARER_MAX_NUMBER;

   // TODO: convert Elementary to subclassing of Evas_Smart_Class
   // TODO: and save some bytes, making descriptions per-class and not instance!
   evas_object_smart_callbacks_descriptions_set(obj, _signals);

   if (!ecore_file_download_protocol_available("http://"))
      ERR("Ecore must be built with curl support for the map widget!");

   return obj;
#else
   (void) parent;
   return NULL;
#endif
}

EAPI void
elm_map_zoom_set(Evas_Object *obj, int zoom)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);
   EINA_SAFETY_ON_NULL_RETURN(wd->src);

   if (wd->mode != ELM_MAP_ZOOM_MODE_MANUAL) return;
   if (zoom < 0) zoom = 0;
   if (wd->zoom == zoom) return;
   if (zoom > wd->src->zoom_max) zoom = wd->src->zoom_max;
   if (zoom < wd->src->zoom_min) zoom = wd->src->zoom_min;

   Delayed_Data *data = ELM_NEW(Delayed_Data);
   data->func = _zoom_set;
   data->wd = wd;
   data->zoom = zoom;
   data->wd->delayed_jobs = eina_list_append(data->wd->delayed_jobs, data);
   evas_object_smart_changed(data->wd->pan_smart);
#else
   (void) obj;
   (void) zoom;
#endif
}

EAPI int
elm_map_zoom_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) 0;
   Widget_Data *wd = elm_widget_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, 0);
   return wd->zoom;
#else
   (void) obj;
   return 0;
#endif
}

EAPI void
elm_map_zoom_mode_set(Evas_Object *obj, Elm_Map_Zoom_Mode mode)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Delayed_Data *data = ELM_NEW(Delayed_Data);
   data->mode = mode;
   data->func = _zoom_mode_set;
   data->wd = wd;
   data->wd->delayed_jobs = eina_list_append(data->wd->delayed_jobs, data);
   evas_object_smart_changed(data->wd->pan_smart);
#else
   (void) obj;
   (void) mode;
#endif
}

EAPI Elm_Map_Zoom_Mode
elm_map_zoom_mode_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) ELM_MAP_ZOOM_MODE_MANUAL;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, ELM_MAP_ZOOM_MODE_MANUAL);

   return wd->mode;
#else
   (void) obj;
   return ELM_MAP_ZOOM_MODE_MANUAL;
#endif
}

EAPI void
elm_map_geo_region_bring_in(Evas_Object *obj, double lon, double lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);

   Delayed_Data *data = ELM_NEW(Delayed_Data);
   data->func = _region_bring_in;
   data->wd = wd;
   data->lon = lon;
   data->lat = lat;
   data->wd->delayed_jobs = eina_list_append(data->wd->delayed_jobs, data);
   evas_object_smart_changed(data->wd->pan_smart);
#else
   (void) obj;
   (void) lon;
   (void) lat;
#endif
}

EAPI void
elm_map_geo_region_show(Evas_Object *obj, double lon, double lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);

   Delayed_Data *data = ELM_NEW(Delayed_Data);
   data->func = _region_show;
   data->wd = wd;
   data->lon = lon;
   data->lat = lat;
   data->wd->delayed_jobs = eina_list_append(data->wd->delayed_jobs, data);
   evas_object_smart_changed(data->wd->pan_smart);
#else
   (void) obj;
   (void) lon;
   (void) lat;
#endif
}

EAPI void
elm_map_geo_region_get(const Evas_Object *obj, double *lon, double *lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   double tlon, tlat;
   Evas_Coord px, py, vw, vh;

   _pan_geometry_get(wd, &px, &py);
   _viewport_size_get(wd, &vw, &vh);
   elm_map_utils_convert_coord_into_geo(obj, vw/2 - px, vh/2 -py, wd->size.w,
                                        &tlon, &tlat);
   if (lon) *lon = tlon;
   if (lat) *lat = tlat;
#else
   (void) obj;
   (void) lon;
   (void) lat;
#endif
}

EAPI void
elm_map_paused_set(Evas_Object *obj, Eina_Bool paused)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   if (wd->paused == !!paused) return;
   wd->paused = !!paused;
   if (wd->paused)
     {
        if (wd->zoom_animator)
          {
             if (wd->zoom_animator) ecore_animator_del(wd->zoom_animator);
             wd->zoom_animator = NULL;
             zoom_do(wd, wd->zoom);
          }
        edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr),
                                "elm,state,busy,stop", "elm");
     }
   else
     {
        if (wd->download_num >= 1)
           edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr),
                                   "elm,state,busy,start", "elm");
     }
#else
   (void) obj;
   (void) paused;
#endif
}

EAPI void
elm_map_paused_markers_set(Evas_Object *obj, Eina_Bool paused)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   if (wd->paused_markers == !!paused) return;
   wd->paused_markers = paused;
#else
   (void) obj;
   (void) paused;
#endif
}

EAPI Eina_Bool
elm_map_paused_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) EINA_FALSE;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EINA_FALSE);

   return wd->paused;
#else
   (void) obj;
   return EINA_FALSE;
#endif
}

EAPI Eina_Bool
elm_map_paused_markers_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) EINA_FALSE;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EINA_FALSE);

   return wd->paused_markers;
#else
   (void) obj;
   return EINA_FALSE;
#endif
}

EAPI void
elm_map_utils_downloading_status_get(const Evas_Object *obj, int *try_num, int *finish_num)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   if (try_num) *try_num = wd->try_num;
   if (finish_num) *finish_num = wd->finish_num;
#else
   (void) obj;
   (void) try_num;
   (void) finish_num;
#endif
}

EAPI void
elm_map_utils_convert_coord_into_geo(const Evas_Object *obj, int x, int y, int size, double *lon, double *lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   int zoom = floor(log(size / 256) / log(2));
   if ((wd->src) && (wd->src->coord_into_geo))
     {
        if (wd->src->coord_into_geo(obj, zoom, x, y, size, lon, lat)) return;
     }

   if (lon) *lon = (x / (double)size * 360.0) - 180;
   if (lat)
     {
        double n = ELM_PI - (2.0 * ELM_PI * y / size);
        *lat = 180.0 / ELM_PI * atan(0.5 * (exp(n) - exp(-n)));
     }
#else
   (void) obj;
   (void) x;
   (void) y;
   (void) size;
   (void) lon;
   (void) lat;
#endif
}

EAPI void
elm_map_utils_convert_geo_into_coord(const Evas_Object *obj, double lon, double lat, int size, int *x, int *y)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   int zoom = floor(log(size / 256) / log(2));
   if ((wd->src) && (wd->src->geo_into_coord))
     {
        if (wd->src->geo_into_coord(obj, zoom, lon, lat, size, x, y)) return;
     }

   if (x) *x = floor((lon + 180.0) / 360.0 * size);
   if (y)
      *y = floor((1.0 - log(tan(lat * ELM_PI / 180.0) + (1.0 / cos(lat * ELM_PI / 180.0)))
                 / ELM_PI) / 2.0 * size);
#else
   (void) obj;
   (void) lon;
   (void) lat;
   (void) size;
   (void) x;
   (void) y;
#endif
}

EAPI Elm_Map_Name *
elm_map_utils_convert_coord_into_name(const Evas_Object *obj, double lon, double lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   return _utils_convert_name(obj, ELM_MAP_NAME_METHOD_REVERSE, NULL, lon, lat);
#else
   (void) obj;
   (void) lon;
   (void) lat;
   return NULL;
#endif
}

EAPI Elm_Map_Name *
elm_map_utils_convert_name_into_coord(const Evas_Object *obj, char *address)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   if (!address) return NULL;
   return _utils_convert_name(obj, ELM_MAP_NAME_METHOD_SEARCH, address, 0.0, 0.0);
#else
   (void) obj;
   (void) address;
   return NULL;
#endif
}

EINA_DEPRECATED EAPI void
elm_map_utils_rotate_coord(const Evas_Object *obj, const Evas_Coord x, const Evas_Coord y, const Evas_Coord cx, const Evas_Coord cy, const double degree, Evas_Coord *xx, Evas_Coord *yy)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   _coord_rotate(x, y, cx, cy, degree, xx, yy);
#else
   (void) x;
   (void) y;
   (void) cx;
   (void) cy;
   (void) degree;
   (void) xx;
   (void) yy;
#endif
}

EAPI void
elm_map_canvas_to_geo_convert(const Evas_Object *obj, Evas_Coord x, Evas_Coord y, double *lon, double *lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(lon);
   EINA_SAFETY_ON_NULL_RETURN(lat);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Evas_Coord px, py, vw, vh;
   _pan_geometry_get(wd, &px, &py);
   _viewport_size_get(wd, &vw, &vh);
   _coord_rotate(x - px, y - py, (vw / 2) - px, (vh / 2) - py, -wd->rotate.d,
                 &x, &y);
   elm_map_utils_convert_coord_into_geo(obj, x, y, wd->size.w, lon, lat);
#else
   (void) obj;
   (void) x;
   (void) y;
   (void) lon;
   (void) lat;
#endif
}

EAPI Elm_Map_Marker *
elm_map_marker_add(Evas_Object *obj, double lon, double lat, Elm_Map_Marker_Class *clas, Elm_Map_Group_Class *group_clas, void *data)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);

   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(clas, NULL);

   Elm_Map_Marker *marker = ELM_NEW(Elm_Map_Marker);
   marker->wd = wd;
   marker->clas = clas;
   marker->group_clas = group_clas;
   marker->longitude = lon;
   marker->latitude = lat;
   marker->data = data;
   marker->x = 0;
   marker->y = 0;
   _edj_marker_size_get(wd, &marker->w, &marker->h);

   marker->obj = elm_layout_add(wd->obj);
   evas_object_smart_member_add(marker->obj, wd->pan_smart);
   evas_object_stack_above(marker->obj, wd->sep_maps_markers);

   edje_object_signal_callback_add(elm_layout_edje_get(marker->obj),
                                   "open", "elm", _marker_bubble_open_cb,
                                   marker);
   edje_object_signal_callback_add(elm_layout_edje_get(marker->obj),
                                   "bringin", "elm", _marker_bringin_cb,
                                   marker);

   wd->markers = eina_list_append(wd->markers, marker);
   if (marker->group_clas) group_clas->markers = eina_list_append(group_clas->markers,
                                                                  marker);
   evas_object_smart_changed(wd->pan_smart);
   return marker;
#else
   (void) obj;
   (void) lon;
   (void) lat;
   (void) clas;
   (void) group_clas;
   (void) data;
   return NULL;
#endif
}

EAPI void
elm_map_marker_remove(Elm_Map_Marker *marker)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(marker);
   Widget_Data *wd = marker->wd;
   EINA_SAFETY_ON_NULL_RETURN(wd);

   if ((marker->content) && (marker->clas->func.del))
      marker->clas->func.del(wd->obj, marker, marker->data, marker->content);

   if (marker->bubble) _bubble_free(marker->bubble);
   if (marker->group) _marker_group_free(marker->group);

   if (marker->group_clas)
      marker->group_clas->markers = eina_list_remove(marker->group_clas->markers, marker);
   wd->markers = eina_list_remove(wd->markers, marker);

   evas_object_del(marker->obj);
   free(marker);

   evas_object_smart_changed(wd->pan_smart);
#else
   (void) marker;
#endif
}

EAPI void
elm_map_marker_region_get(const Elm_Map_Marker *marker, double *lon, double *lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(marker);
   if (lon) *lon = marker->longitude;
   if (lat) *lat = marker->latitude;
#else
   (void) marker;
   (void) lon;
   (void) lat;
#endif
}

EAPI void
elm_map_marker_bring_in(Elm_Map_Marker *marker)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(marker);
   elm_map_geo_region_bring_in(marker->wd->obj, marker->longitude, marker->latitude);
#else
   (void) marker;
#endif
}

EAPI void
elm_map_marker_show(Elm_Map_Marker *marker)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(marker);
   elm_map_geo_region_show(marker->wd->obj, marker->longitude, marker->latitude);
#else
   (void) marker;
#endif
}

EAPI void
elm_map_markers_list_show(Eina_List *markers)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(markers);
   EINA_SAFETY_ON_TRUE_RETURN(!eina_list_count(markers));

   Elm_Map_Marker *marker;
   marker = eina_list_data_get(markers);

   Delayed_Data *data = ELM_NEW(Delayed_Data);
   data->func = _marker_list_show;
   data->wd = marker->wd;
   data->markers = eina_list_clone(markers);
   data->wd->delayed_jobs = eina_list_append(data->wd->delayed_jobs, data);
   evas_object_smart_changed(data->wd->pan_smart);
#else
   (void) markers;
#endif
}

EAPI void
elm_map_max_marker_per_group_set(Evas_Object *obj, int max)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   wd->markers_max_num = max;
#else
   (void) obj;
   (void) max;
#endif
}

EAPI Evas_Object *
elm_map_marker_object_get(const Elm_Map_Marker *marker)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN_VAL(marker, NULL);
   return marker->content;
#else
   (void) marker;
   return NULL;
#endif
}

EAPI void
elm_map_marker_update(Elm_Map_Marker *marker)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(marker);
   Widget_Data *wd = marker->wd;
   EINA_SAFETY_ON_NULL_RETURN(wd);

   _marker_update(marker);
#else
   (void) marker;
#endif
}

EAPI void
elm_map_bubbles_close(Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Eina_List *l;
   Elm_Map_Marker *marker;
   EINA_LIST_FOREACH(wd->markers, l, marker)
     {
        if (marker->bubble) _bubble_free(marker->bubble);
        marker->bubble = NULL;

        if (marker->group)
          {
             if (marker->group->bubble) _bubble_free(marker->group->bubble);
             marker->group->bubble = NULL;
          }
     }
#else
   (void) obj;
#endif
}

EAPI Elm_Map_Group_Class *
elm_map_group_class_new(Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);

   Elm_Map_Group_Class *clas = ELM_NEW(Elm_Map_Group_Class);
   clas->wd = wd;
   clas->zoom_displayed = 0;
   clas->zoom_grouped = 255;
   eina_stringshare_replace(&clas->style, "radio");

   wd->group_classes = eina_list_append(wd->group_classes, clas);

   return clas;
#else
   (void) obj;
   return NULL;
#endif
}

EAPI void
elm_map_group_class_style_set(Elm_Map_Group_Class *clas, const char *style)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   eina_stringshare_replace(&clas->style, style);
#else
   (void) clas;
   (void) style;
#endif
}

EAPI void
elm_map_group_class_icon_cb_set(Elm_Map_Group_Class *clas, ElmMapGroupIconGetFunc icon_get)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   clas->func.icon_get = icon_get;
#else
   (void) clas;
   (void) icon_get;
#endif
}

EAPI void
elm_map_group_class_data_set(Elm_Map_Group_Class *clas, void *data)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   clas->data = data;
#else
   (void) clas;
   (void) data;
#endif
}

EAPI void
elm_map_group_class_zoom_displayed_set(Elm_Map_Group_Class *clas, int zoom)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   clas->zoom_displayed = zoom;
#else
   (void) clas;
   (void) zoom;
#endif
}

EAPI void
elm_map_group_class_zoom_grouped_set(Elm_Map_Group_Class *clas, int zoom)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   clas->zoom_grouped = zoom;
#else
   (void) clas;
   (void) zoom;
#endif
}

EAPI void
elm_map_group_class_hide_set(Evas_Object *obj, Elm_Map_Group_Class *clas, Eina_Bool hide)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);
   EINA_SAFETY_ON_NULL_RETURN(clas);

   clas->hide = hide;
   evas_object_smart_changed(wd->pan_smart);
#else
   (void) obj;
   (void) clas;
   (void) hide;
#endif
}

EAPI Elm_Map_Marker_Class *
elm_map_marker_class_new(Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);

   Elm_Map_Marker_Class *clas = ELM_NEW(Elm_Map_Marker_Class);
   eina_stringshare_replace(&clas->style, "radio");

   wd->marker_classes = eina_list_append(wd->marker_classes, clas);
   return clas;
#else
   (void) obj;
   return NULL;
#endif
}

EAPI void
elm_map_marker_class_style_set(Elm_Map_Marker_Class *clas, const char *style)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   eina_stringshare_replace(&clas->style, style);
#else
   (void) clas;
   (void) style;
#endif
}

EAPI void
elm_map_marker_class_icon_cb_set(Elm_Map_Marker_Class *clas, ElmMapMarkerIconGetFunc icon_get)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   clas->func.icon_get = icon_get;
#else
   (void) clas;
   (void) icon_get;
#endif
}

EAPI void
elm_map_marker_class_get_cb_set(Elm_Map_Marker_Class *clas, ElmMapMarkerGetFunc get)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   clas->func.get = get;
#else
   (void) clas;
   (void) get;
#endif
}

EAPI void
elm_map_marker_class_del_cb_set(Elm_Map_Marker_Class *clas, ElmMapMarkerDelFunc del)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(clas);
   clas->func.del = del;
#else
   (void) clas;
   (void) del;
#endif
}

EAPI const char **
elm_map_source_names_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);

   return wd->src_names;
#else
   (void) obj;
   return NULL;
#endif
}

EAPI void
elm_map_source_name_set(Evas_Object *obj, const char *source_name)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   Map_Sources_Tab *s;
   Eina_List *l;
   int zoom;

   if (wd->src)
     {
        if (!strcmp(wd->src->name, source_name)) return;
        if (!wd->src->url_cb) return;
     }

   _grid_all_clear(wd);
   EINA_LIST_FOREACH(wd->srcs, l, s)
     {
        if (!strcmp(s->name, source_name))
          {
             wd->src = s;
             break;
          }
     }
   zoom = wd->zoom;
   wd->zoom = -1;

   if (wd->src)
     {
        if (wd->src->zoom_max < zoom)
          zoom = wd->src->zoom_max;
        if (wd->src->zoom_min > zoom)
          zoom = wd->src->zoom_min;
        if (wd->src->zoom_max < wd->zoom_max) wd->zoom_max = wd->src->zoom_max;
        if (wd->src->zoom_min > wd->zoom_min) wd->zoom_min = wd->src->zoom_min;
     }
   _grid_all_create(wd);
   zoom_do(wd, zoom);
#else
   (void) obj;
   (void) source_name;
#endif
}

EAPI const char *
elm_map_source_name_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);

   if ((!wd) || (!wd->src)) return NULL;
   return wd->src->name;
#else
   (void) obj;
   return NULL;
#endif
}

EAPI void
elm_map_route_source_set(Evas_Object *obj, Elm_Map_Route_Sources source)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   wd->route_source = source;
#else
   (void) obj;
   (void) source;
#endif
}

EAPI Elm_Map_Route_Sources
elm_map_route_source_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) ELM_MAP_ROUTE_SOURCE_YOURS;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, ELM_MAP_ROUTE_SOURCE_YOURS);

   return wd->route_source;
#else
   (void) obj;
   return ELM_MAP_ROUTE_SOURCE_YOURS;
#endif
}

EAPI void
elm_map_source_zoom_max_set(Evas_Object *obj, int zoom)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);
   EINA_SAFETY_ON_NULL_RETURN(wd->src);

   if ((zoom > wd->src->zoom_max) || (zoom < wd->src->zoom_min)) return;
   wd->zoom_max = zoom;
#else
   (void) obj;
   (void) zoom;
#endif
}

EAPI int
elm_map_source_zoom_max_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) 18;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, -1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd->src, -1);

   return wd->zoom_max;
#else
   (void) obj;
   return 18;
#endif
}

EAPI void
elm_map_source_zoom_min_set(Evas_Object *obj, int zoom)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);
   EINA_SAFETY_ON_NULL_RETURN(wd->src);

   if ((zoom > wd->src->zoom_max) || (zoom < wd->src->zoom_min)) return;
   wd->zoom_min = zoom;
#else
   (void) obj;
   (void) zoom;
#endif
}

EAPI int
elm_map_source_zoom_min_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) 0;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, -1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd->src, -1);

   return wd->zoom_min;
#else
   (void) obj;
   return 0;
#endif
}

EAPI void
elm_map_user_agent_set(Evas_Object *obj, const char *user_agent)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);
   EINA_SAFETY_ON_NULL_RETURN(user_agent);

   eina_stringshare_replace(&wd->user_agent, user_agent);

   if (!wd->ua) wd->ua = eina_hash_string_small_new(NULL);
   eina_hash_set(wd->ua, "User-Agent", wd->user_agent);
#else
   (void) obj;
   (void) user_agent;
#endif
}

EAPI const char *
elm_map_user_agent_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);

   return wd->user_agent;
#else
   (void) obj;
   return NULL;
#endif
}

EAPI Elm_Map_Route *
elm_map_route_add(Evas_Object *obj, Elm_Map_Route_Type type, Elm_Map_Route_Method method, double flon, double flat, double tlon, double tlat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);
   char buf[PATH_MAX];
   char *source;
   char *type_name = NULL;
   int fd;

   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd->src, NULL);

   Elm_Map_Route *route = ELM_NEW(Elm_Map_Route);

   snprintf(buf, sizeof(buf), DEST_ROUTE_XML_FILE);
   fd = mkstemp(buf);
   if (fd < 0)
     {
        free(route);
        return NULL;
     }

   route->con_url = ecore_con_url_new(NULL);
   route->ud.fname = strdup(buf);
   INF("xml file : %s", route->ud.fname);

   route->ud.fd = fdopen(fd, "w+");
   if ((!route->con_url) || (!route->ud.fd))
     {
        ecore_con_url_free(route->con_url);
        free(route);
        return NULL;
     }

   route->wd = wd;
   route->color.r = 255;
   route->color.g = 0;
   route->color.b = 0;
   route->color.a = 255;
   route->handlers = eina_list_append
     (route->handlers, (void *)ecore_event_handler_add
         (ECORE_CON_EVENT_URL_COMPLETE, _route_complete_cb, route));

   route->inbound = EINA_FALSE;
   route->type = type;
   route->method = method;
   route->flon = flon;
   route->flat = flat;
   route->tlon = tlon;
   route->tlat = tlat;

   switch (type)
     {
      case ELM_MAP_ROUTE_TYPE_MOTOCAR:
        type_name = strdup(ROUTE_TYPE_MOTORCAR);
        break;
      case ELM_MAP_ROUTE_TYPE_BICYCLE:
        type_name = strdup(ROUTE_TYPE_BICYCLE);
        break;
      case ELM_MAP_ROUTE_TYPE_FOOT:
        type_name = strdup(ROUTE_TYPE_FOOT);
        break;
      default:
        break;
     }

   source = wd->src->route_url_cb(obj, type_name, method, flon, flat, tlon, tlat);
   INF("route url = %s", source);

   wd->route = eina_list_append(wd->route, route);

   ecore_con_url_url_set(route->con_url, source);
   ecore_con_url_fd_set(route->con_url, fileno(route->ud.fd));
   ecore_con_url_data_set(route->con_url, route);
   ecore_con_url_get(route->con_url);
   if (type_name) free(type_name);
   if (source) free(source);

   edje_object_signal_emit(elm_smart_scroller_edje_object_get(wd->scr),
                           "elm,state,busy,start", "elm");
   evas_object_smart_callback_call(wd->obj, SIG_ROUTE_LOAD, NULL);
   return route;
#else
   (void) obj;
   (void) type;
   (void) method;
   (void) flon;
   (void) flat;
   (void) tlon;
   (void) tlat;
   return NULL;
#endif
}

EAPI void
elm_map_route_remove(Elm_Map_Route *route)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(route);

   Path_Waypoint *w;
   Path_Node *n;
   Evas_Object *p;
   Ecore_Event_Handler *h;

   EINA_LIST_FREE(route->path, p)
     {
        evas_object_del(p);
     }

   EINA_LIST_FREE(route->waypoint, w)
     {
        if (w->point) eina_stringshare_del(w->point);
        free(w);
     }

   EINA_LIST_FREE(route->nodes, n)
     {
        if (n->pos.address) eina_stringshare_del(n->pos.address);
        free(n);
     }

   EINA_LIST_FREE(route->handlers, h)
     {
        ecore_event_handler_del(h);
     }

   if (route->ud.fname)
     {
        ecore_file_remove(route->ud.fname);
        free(route->ud.fname);
        route->ud.fname = NULL;
     }
#else
   (void) route;
#endif
}

EAPI void
elm_map_route_color_set(Elm_Map_Route *route, int r, int g , int b, int a)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(route);
   route->color.r = r;
   route->color.g = g;
   route->color.b = b;
   route->color.a = a;
#else
   (void) route;
   (void) r;
   (void) g;
   (void) b;
   (void) a;
#endif
}

EAPI void
elm_map_route_color_get(const Elm_Map_Route *route, int *r, int *g , int *b, int *a)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(route);
   if (r) *r = route->color.r;
   if (g) *g = route->color.g;
   if (b) *b = route->color.b;
   if (a) *a = route->color.a;
#else
   (void) route;
   (void) r;
   (void) g;
   (void) b;
   (void) a;
#endif
}

EAPI double
elm_map_route_distance_get(const Elm_Map_Route *route)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN_VAL(route, 0.0);
   return route->info.distance;
#else
   (void) route;
   return 0.0;
#endif
}

EAPI const char*
elm_map_route_node_get(const Elm_Map_Route *route)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN_VAL(route, NULL);
   return route->info.nodes;
#else
   (void) route;
   return NULL;
#endif
}

EAPI const char*
elm_map_route_waypoint_get(const Elm_Map_Route *route)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN_VAL(route, NULL);
   return route->info.waypoints;
#else
   (void) route;
   return NULL;
#endif
}

EAPI const char *
elm_map_name_address_get(const Elm_Map_Name *name)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN_VAL(name, NULL);
   return name->address;
#else
   (void) name;
   return NULL;
#endif
}

EAPI void
elm_map_name_region_get(const Elm_Map_Name *name, double *lon, double *lat)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(name);
   if (lon) *lon = name->lon;
   if (lat) *lat = name->lat;
#else
   (void) name;
   (void) lon;
   (void) lat;
#endif
}

EAPI void
elm_map_name_remove(Elm_Map_Name *name)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   EINA_SAFETY_ON_NULL_RETURN(name);
   if (name->address)
     {
        free(name->address);
        name->address = NULL;
     }
   if (name->handler)
     {
        ecore_event_handler_del(name->handler);
        name->handler = NULL;
     }
   if (name->ud.fname)
     {
        ecore_file_remove(name->ud.fname);
        free(name->ud.fname);
        name->ud.fname = NULL;
     }
#else
   (void) name;
#endif
}

EAPI void
elm_map_rotate_set(Evas_Object *obj, double degree, Evas_Coord cx, Evas_Coord cy)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   wd->rotate.d = degree;
   wd->rotate.cx = cx;
   wd->rotate.cy = cy;

   evas_object_smart_changed(wd->pan_smart);
#else
   (void) obj;
   (void) degree;
   (void) cx;
   (void) cy;
#endif
}

EAPI void
elm_map_rotate_get(const Evas_Object *obj, double *degree, Evas_Coord *cx, Evas_Coord *cy)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   if (degree) *degree = wd->rotate.d;
   if (cx) *cx = wd->rotate.cx;
   if (cy) *cy = wd->rotate.cy;
#else
   (void) obj;
   (void) degree;
   (void) cx;
   (void) cy;
#endif
}

EAPI void
elm_map_wheel_disabled_set(Evas_Object *obj, Eina_Bool disabled)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   if ((!wd->wheel_disabled) && (disabled))
     evas_object_event_callback_del_full(obj, EVAS_CALLBACK_MOUSE_WHEEL, _mouse_wheel_cb, obj);
   else if ((wd->wheel_disabled) && (!disabled))
     evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_WHEEL, _mouse_wheel_cb, obj);
   wd->wheel_disabled = !!disabled;
#else
   (void) obj;
   (void) disabled;
#endif
}

EAPI Eina_Bool
elm_map_wheel_disabled_get(const Evas_Object *obj)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) EINA_FALSE;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EINA_FALSE);

   return wd->wheel_disabled;
#else
   (void) obj;
   return EINA_FALSE;
#endif
}

#ifdef ELM_EMAP
EAPI Evas_Object *
elm_map_track_add(Evas_Object *obj, EMap_Route *emap)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) NULL;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wd, EINA_FALSE);

   Evas_Object *route = elm_route_add(obj);
   elm_route_emap_set(route, emap);
   wd->track = eina_list_append(wd->track, route);

   return route;
#else
   (void) obj;
   (void) emap;
   return NULL;
#endif
}
#endif

EAPI void
elm_map_track_remove(Evas_Object *obj, Evas_Object *route)
{
#ifdef HAVE_ELEMENTARY_ECORE_CON
   ELM_CHECK_WIDTYPE(obj, widtype) ;
   Widget_Data *wd = elm_widget_data_get(obj);
   EINA_SAFETY_ON_NULL_RETURN(wd);

   wd->track = eina_list_remove(wd->track, route);
   evas_object_del(route);
#else
   (void) obj;
   (void) route;
#endif
}
