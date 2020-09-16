/*********************************************************
	 ---   My Module Specific Messages ---

See IPC documentation for more information:
http://www.cs.cmu.edu/~ipc/

*********************************************************/

#ifndef CARMEN_CVIS_MESSAGES_H
#define CARMEN_CVIS_MESSAGES_H

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct {
  int width;					/* The x dimension of the image in pixels */
  int height;					/* The y dimension of the image in pixels */
  int image_size;				/* width*height*bytes_per_pixel */
  unsigned char *raw_image;
  double timestamp;
  char *host;
} carmen_navigator_gui2_map_view_message;


#define      CARMEN_NAVIGATOR_GUI2_MAP_VIEW_NAME       "carmen_navigator_gui2_map_view"
#define      CARMEN_NAVIGATOR_GUI2_MAP_VIEW_FMT        "{int,int,int,<ubyte:3>,double,string}"


#ifdef __cplusplus
}
#endif

#endif

// @}
