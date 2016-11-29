/*
 * Copyright (C) 2008  Axis Communications AB
 */

/**
 *
 *  @file capture.h
 *
 *  @brief Capture Interface, for reading images.
 *
 * The capture interface contains functions for opening and closing an image stream
 * and for reading image frames from the stream.
 *
 * The media_type parameter of the capture_open_stream function defines what
 * kind of stream to open. Right now It can be \c IMAGE_JPEG for a JPEG stream or
 * \c IMAGE_UNCOMPRESSED for a stream of uncompressed YUV images. The
 * default YUV type is planar I420 but other formats can be specified
 * by using the \c media_props argument to \c capture_open_stream().
 *
 * \section media_props Stream properties
 * The \c media_props argument is an extended version of the VAPIX option
 * string. The following options are availble for the different stream types.
 *
 * \subsection img_jpg IMAGE_JPEG
 * For JPEG images the \c media_props argument accepts the same kind of parameters
 * as specified in the VAPIX. 
 *
 * Read more at http://www.axis.com/techsup/cam_servers/dev/cam_http_api_2.php#api_blocks_image_video_mjpg_video
 *
 * \subsection img_yuv IMAGE_UNCOMPRESSED
 * For uncompressed YUV images the following parameters are defined:
 * - fps=\<int\>
 * - sdk_format=\<string\>
 * - resolution=\<string\>
 *
 * The \b fps parameter specifies the frame rate of the images being pushed out on the stream.
 *
 * The \b sdk_format string is in \e FOURCC format. A \e FOURCC is a sequence of four bytes used do identify data formats.
 *  Supported formats include:
 *  - Y800
 *  - I420
 *  - UYVY
 *
 *  For ARTPEC-3 and ARTPEC-4 based products UYVY is native and require no internal conversion
 *
 * The \b resolution string specifies the dimensions of the images in the stream.
 * The syntax is the same as the corresponding VAPIX parameter.
 *
 * \subsection gen General properties
 *
 * The \b sdk_burst-length parameter will set the stream to operate in \e burst-mode. To receive frames in burst mode use \c capture_get_burst().
 * If a sdk_burst-length of 5 is specified, 5 frames are available and read when \c capture_get_burst() is called.
 * The rate of which \c capture_get_burst() gets called will determine the rate between the bursts and the fps specified on the stream will determine the rate
 * between the frames in a burst. Frames in different bursts may overlap but there will be at least one new frame in each individual burst.
 *
 * \subsection crop Cropping
 * The new VAPIX parameters \e cropsize and \e croppos can be used to read a cropped area from an image.
 * To read an area of 160x120 from an 640x480 image, apply the VAPIX parameters:
 * \code
 *  ?resolution=640x480&croppos=100,0&cropsize=160x120
 * \endcode
 *
 * \subsection stride Stride
 *  To support the processing enhancements provided by the RAPP library, uncompressed Y800 images will consist of aligned image data. And the interface defines a row length, called stride.
 * Each uncompressed Y800 image is described by data, width, height and stride:

 * Pointer to the pixel data. The pixel data buffer is aligned.
 * The alignment number is platform specific and compatible with the RAPP library.
 * - capture_frame_data()
 *
 * The width of the image in number of pixels.
 * - capture_frame_width()
 *
 *    The height of the image in number of pixels.
 * - capture_frame_height()
 *
 *  The length of the rows in the image in bytes. The stride value is aligned and can be greater than the image width.\n
 *  The stride of the image needs to be taken into account when working with Y800 images. After the first \e width bytes of image data, \n
 *  the row is padded to achieve the stride. When a Y800 image with a resolution of 350x288 is requested each row has a length of 350  \n
 *  bytes image data and then a 2 byte padding, giving a stride of 352.
 * - capture_frame_stride()
 *

 * \subsection native Architecture dependent interface
 * There is also an architecture dependent interface, called native. It can be used to read YUV images in the format native to the current hardware. (UYVY on artpec-3 and artpec-4, NV12 on ambarella-a5s)
 * Through the interface, width and height is specified and a pointer to a buffer of YUV data can be read. The memory for the buffer is reused and it needs to be copied
 * if it is to be saved. If the application need to be fast and the format of YUV or the need to use VAPIX functionality like cropping is not important, then this interface can be useful
 *
 *
 * \section Examples
 * In this section some examples of how to use the Capture interface is shown.
 *
 * \subsection ex1 Example 1
 * This will request a stream of JPEG images, with a framerate of 10 images per second.
 * \code
   media_stream *stream = capture_open_stream(IMAGE_JPEG, "fps=10");
 \endcode
 *
 * \subsection ex2 Example 2
 * A more complete example of how to use the capture interface.
 * This will request a stream of Y800 uncompressed images, the different \c media_props options
 * are also documented in the VAPIX API at http://www.axis.com/techsup/cam_servers/dev/cam_http_api_index.php
 *
 * The code will retrieve a frame of Y800 data and write it as a pgm-file. The stride is needed to make sure the image gets written correctly.
 \code
 static void write_pgm(void *data, int width, int height, int stride) {
  FILE *fp;
  int row, column;

  fp = fopen("test.pgm", "w");

  fprintf(fp, "P5\n");
  fprintf(fp, "# CREATOR: Axis Communications AB\n");
  fprintf(fp, "%d %d\n", width, height);
  fprintf(fp, "%d\n", 255);


  for (row = 0; row < height; row++)
    for (column = 0; column < width; column++)
      fputc (((unsigned char *) data)[row * stride + column], fp);

  fclose(fp);
}

 int main (int argc, char **argv) {
  media_frame  *frame;
  void     *data;
  size_t   size;
  media_stream *stream;
  capture_time timestamp;

  stream = capture_open_stream(IMAGE_UNCOMPRESSED, "fps=25&sdk_format=Y800&resolution=350x288&rotation=180");
  frame  = capture_get_frame(stream);

  data      = capture_frame_data(frame);
  width     = capture_frame_width(frame);
  height    = capture_frame_height(frame);
  stride    = capture_frame_stride(frame);
  timestamp = capture_frame_timestamp(frame);

  printf("Frame captured at %" CAPTURE_TIME_FORMAT "\n", timestamp);
  write_pgm(data, width, height, stride);

  capture_frame_free(frame);
  capture_close_stream(stream);

  return 0;
}
 \endcode
 *
 *\subsection ex3 Example 3
 * An example of how to use the capture interface in burst-mode.
 *
 \code
  media_frame  **burst;
  media_stream *stream;
  int i;

  stream = capture_open_stream(IMAGE_UNCOMPRESSED, "fps=25&sdk_format=Y800&resolution=160x120&sdk_burst-length=5");
  burst  = capture_get_burst (stream);

  for (i = 0; i < 5; i++) {
    process_burst_frame (burst[i]);
    capture_frame_free (burst[i]);
  }
  capture_burst_free (burst);
  capture_close_stream(stream);
 \endcode

  *\subsection ex4 Example 4
 * An example of how to use the architecture dependent interface.
 *
 * \code
 * media_native *nat = capture_open_native (640, 480);
 * capture_start_native (nat);
 *
 * unsigned char *data = capture_get_image_native (nat);
 * process_data (data);
 * capture_close_native (nat);
 *
 * \endcode

 *
 * \section Compilation
 * To build a program using the capture interface you need to supply the library name as \c -lcapture.
 * And you need to include the header file \c capture.h.
 *
* \section compile_for_host Compilation for a linux host
  * The capture interface is available for development and debugging in a PC environment.
  * The library is called capturehost (-lcapturehost)and the interface can read existing mjpeg files.
  * Needed are a Linux PC and an Axis camera or video encoder connected to the PC via LAN.
  * \subsection restrictionsh Restrictions for host
  *
  *  Supported formats are:
  *  - Y800
  *  - I420
  *
  * \subsection ex5 Example host
  * This is an example of how to use the capture interface on the host.
  \code
   media_frame  *frame;
   void     *data;
   size_t   size;
   media_stream *stream;
   capture_time timestamp;

   stream = capture_open_stream(IMAGE_JPEG, "capture-cameraIP=\<IP\>&capture-userpass=\<user\>:\<password\>sdk_format=Y800&&resolution=176x144&fps=1"); 	 
   frame  = capture_get_frame(stream);

   data      = capture_frame_data(frame);
   size      = capture_frame_size(frame);
   timestamp = capture_frame_timestamp(frame);

   printf("Frame captured at %" CAPTURE_TIME_FORMAT "\n", timestamp);
   process_data(data, size);

   capture_frame_free(frame);
   capture_close_stream(stream);
  \endcode
  * Note 1: replace \<IP\> with the IP address of the camera.
  Replace \<user\> with an existing user, and \<password\> with the password of this user.
  *
  * Note 2: the capture-camera must be the first item in media_props, capture-pass the second, and followed by all other media_props
  *
  *
  * Note 3: The capturehost interface will use an http proxy whenever the environment variable http_proxy is set using
  * the same principle as curl.
  *
  * \subsection ex5 Example using capturehost with an existing mjpeg file
  * This is an example of how to use the capturehost interface with an existing mjpeg file.
  *
  * First, acquire an mjpeg file from a camera. 	 
  \code
   % curl -s -S -u \<user\>:\<password\> "http://\<IP\>/mjpg/video.mjpg?duration=5&resolution=176x144&fps=5" >myfile.mjpeg
   % # Replace \<user\> with an existing user, and \<password\> with the password of the user:
   % curl -s -S -u viewer:secretpassword "http://192.168.0.90/mjpg/video.mjpg?duration=5&resolution=176x144&fps=5" >myfile.mjpeg
   \endcode
  *
  * Then use this mjpeg file to retrieve gray scale frames.
  \code
  stream = capture_open_stream(IMAGE_UNCOMPRESSED, "capture-cameraIP=myfile.mjpeg&sdk_format=Y800&fps=1");
  stream = capture_open_stream(IMAGE_UNCOMPRESSED, "capture-cameraIP=myfile.mjpeg&sdk_format=I420&fps=1");
  \endcode

  * The capturehost interface is able to reduce the frame rate of an existing mjpeg file selecting a few frames and ignoring
  * the rest, for example, 5 fps can be reduced 1 fps by picking 1 frame and ignoring 4 frames.
  * The capturehost interface can simulate real-time timing of the requested fps by blocking the caller a short while and
  * downscaling the resolution by skipping pixels.

  * On host it is also possible to read a mjeg file from STDIN. In this case the call to capture_open_stream() takes no capture-cameraIP parameter.
  \code
  stream = capture_open_stream(IMAGE_JPEG, "fps=1");
  \endcode

  * For example to use the file sunset.mjpeg with the host program 'my_host_viewer' reading from STDIN:
  \code
  ./my_host_viewer < sunset.mjpeg
  \endcode
 */

#ifndef _CAPTURE_H_
#define _CAPTURE_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdlib.h>

 /** @def IMAGE_JPEG
 * @brief The media_type for a JPEG stream.
 */
#define IMAGE_JPEG "image/jpeg"

/** @def IMAGE_UNCOMPRESSED
 * @brief The media_type for an uncompressed YUV stream.
 */
#define IMAGE_UNCOMPRESSED "video/x-raw-yuv"

typedef struct _stream_info stream_info;
typedef struct _media_stream media_stream;
typedef struct _media_frame media_frame;
typedef struct _media_native media_native;

/** @typedef capture_time
 *  @brief The datatype to hold a timestamp for the time of capture for a frame, measured in nanoseconds for the system uptime (CLOCK_MONOTONIC).
 */
typedef unsigned long long capture_time;

/** @def CAPTURE_TIME_FORMAT
 * @brief The format used when printing capture_time
 */
#define CAPTURE_TIME_FORMAT "llu"

/*
 * -------------------------------------------------------------
 *  Exported functions
 * -------------------------------------------------------------
 */



/** 
 * @brief Opens a new stream of the specified media type with the specified
 * properties.
 *
 * The function will open a stream of the specified media type in the camera.
 * The stream will be taken from an universal cache. If the media type with the
 * specified properties already is running in the camera, they will share the
 * data.
 *
 * @param media_type The specified media type of the stream.
 *
 * @param media_props The properties of the media type, represented as VAPIX 
 * option string. 
 *
 * @return A structure associated with the stream.
 */
media_stream *
capture_open_stream(const char *media_type, const char *media_props);

/**
 * @brief Read a media_frame from an open stream. 
 *
 * The function will get a frame of data from the stream and return it. 
 * The frame contains the data, the size and the timestamp. 
 * The frame needs to be freed after use, using capture_frame_free.
 *
 * @param stream The structure associated with the stream.
 * 
 * 
 * @return A pointer to the data frame.
 */
media_frame *
capture_get_frame(media_stream *stream);



/**
 * @brief Read a burst of media_frames from the stream.
 *
 * The function will get a burst of frames from the stream, the amount of
 * frames is determined by the sdk_burst-length property to the stream.
 *
 * @param stream The stream to read from
 *
 * @return An array of media_frame *
 */
media_frame **
capture_get_burst(media_stream *stream);

/**
 * @brief Obtain the data from the media_frame.
 *
 * @param frame The media_frame to obtain data from.
 *
 * @return A pointer to the data, NULL if frame is NULL.
 */
void *
capture_frame_data(const media_frame *frame);

/**
 * @brief Obtain the data size from the media_frame.
 *
 * @param frame The media_frame to obtain data size from.
 *
 * @return The size of the data, 0 if frame is NULL.
 */
size_t
capture_frame_size(const media_frame *frame);

/**
 * @brief Obtain the timestamp of the media_frame, measured in nanoseconds.
 *
 * The returned value is nanoseconds for the system uptime (CLOCK_MONOTONIC).
 *
 * If the frame is jpeg, the same information is found in the jpeg header.
 *
 * Additional note: if there are 2 different stream used, with different resolution
 * or framerate, then there is no guarantee, that both streams deliver frames
 * which have the same source image (from the sensor or camera).
 * But if the timestamps are identical, then they have the same source image.
 *
 * @param frame The media_frame to obtain timestamp from.
 *
 * @return The timestamp of the data, 0 if frame is NULL.
 */
capture_time
capture_frame_timestamp(const media_frame *frame);

/**
 * @brief Obtain the height of the media_frame.
 *
 * @param frame The media_frame to obtain height from.
 *
 * @return The height of the frame, 0 if frame is NULL.
 */
size_t
capture_frame_height(const media_frame *frame);

/**
 * @brief Obtain the width of the media_frame.
 *
 * @param frame The media_frame to obtain width from.
 *
 * @return The width of the frame, 0 if frame is NULL.
 */
size_t
capture_frame_width(const media_frame *frame);

/**
 * @brief Obtain the stride length of the Y800 media_frame.
 *
 * @param frame The media_frame to obtain stride length from.
 *
 * @return The stride length of the frame, 0 if frame is NULL or not Y800.
 */
size_t
capture_frame_stride(const media_frame *frame);

/**
 * @brief Free the media_frame retrieved from capture_get_frame().
 *
 * @param frame pointer to the media_frame received from capture_get_frame()
 * 
 * @return void
 */
void 
capture_frame_free(media_frame *frame);

/**
 * @brief Free a burst of media_frames.
 *
 * Please note that this fucntion will only free the the array holding the
 * frames. The individual frames are owned by the user of the library and should
 * be freed using capture_frame_free().
 *
 * @param frames the burst (array) of frames to free
 */
void
capture_burst_free (media_frame **frames);

/** 
 * @brief Close a running stream.
 *
 * The function closes the specified stream.
 * 
 * @param stream The structure associated with the stream.
 */
void
capture_close_stream(media_stream *stream);

/**
 * @brief Get information about the stream currently running on the camera.
 *
 * @return A NULL terminated array of stream_info.
 */
stream_info **
capture_stream_info_get ();


/**
 * @brief Obtain the properties of the stream.
 *
 * @param info The stream_info to obtain the properties of.
 * @return The properties as a media_props option string.
 */
char *
capture_stream_info_props (const stream_info *info);

/**
 * @brief Obtain the type of the stream.
 *
 * @param info The stream_info to obtain the type of.
 * @return The properties as a media_type string.
 */
char *
capture_stream_info_type (const stream_info *info);

/**
 * @brief Free the array of stream_info acquired by capture_strem_info_get.
 *
 * @param info The stream_info array.
 */
void
capture_stream_info_free (stream_info **info);

/**
 * @brief Open a connection to retrieve architecture dependent YUV images.
 *
 * @param width The width of the image.
 * @param height The height of the image.
 *
 * @return A media_native object.
 */
media_native *
capture_open_native (int width, int height);

/**
 * @brief Add an initial capture request for a native image
 *
 * @param nat The media_native object.
 *
 * */
void
capture_start_native (media_native *nat);

/**
 * @brief Read an architecture dependent YUV image.
 *
 * The memory will be re-used for each new call to this function. If you need to store the image you have to copy it!
 * This will block until an image is ready and then add a new capture request.
 *
 * @param nat The media_native object.
 *
 * @return A pointer to the YUV image.
 */
void *
capture_get_image_native (media_native *nat);

/**
 * @brief Close connection to native image reading.
 *
 * This will free the memory used to buffer an image.
 *
 * @param nat The media_native object.
 */
void
capture_close_native (media_native *nat);


#ifdef  __cplusplus
}
#endif

#endif
