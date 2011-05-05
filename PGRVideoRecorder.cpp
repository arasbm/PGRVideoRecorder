/*
 * 		PGRVideoRecorder.cpp
 *
 *  	Created on: 2011-04-28
 *      Author: Aras Balali Moghaddam
 *      arasbm >at< gmail >dot< com
 *
 *	This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gthread.h>
#include <glib.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include "FlyCapture2.h"

using namespace std;
using namespace FlyCapture2;

//global variables
unsigned int default_fps = 30;
char* video_file_path = "capture.avi";
PGRGuid pgrGuid;
static bool capture = false;
static bool close_avi = false;
BusManager busManager;

static void HandleError(Error error) {
	g_print("************************\n");
	g_print(error.GetDescription());
	g_print("...\n");
	g_print(error.GetFilename());
	g_print("\n************************\n");
}
/**
 * Connect to PGR cameras and get the info
 * */
static int connect( GtkWidget *widget, gpointer   data )
{
    g_print ("Connecting ...\n");
    unsigned int numOfCam;
    Error error = busManager.GetNumOfCameras(&numOfCam);
	if (error != PGRERROR_OK)
	{
		HandleError( error );
		return -1;
	}

	if ( numOfCam == 0 )
	{
		g_print( "There are NO cameras to connect to.\n" );
		return -1;
	} else if ( numOfCam == 1 ) {
		g_print( "One PGR camera is detected. \n");
	} else {
		g_print( "%u cameras are detected\n", numOfCam );
	}

	return 0;
}

static void startCapture() {
	capture = true;
	g_print("Start recording ...");
}

/**
 * recording thread function
 * */
void* captureFromCamera(void* args) {
	Error error;
	g_print( "started capture thread for camera %u.\n", 0 );
	error = busManager.GetCameraFromIndex( 0, &pgrGuid );
	if ( error != PGRERROR_OK )
	{
	   HandleError( error );
	}

	Camera camera;
	error = camera.Connect(&pgrGuid);
	if (error != PGRERROR_OK)
	{
		HandleError( error );
	}

	error = camera.StartCapture();
	if (error != PGRERROR_OK)
	{
		HandleError( error );
	}

	AVIRecorder recorder;
	AVIOption option;
	PropertyInfo propInfo;
	propInfo.type = FRAME_RATE;
	error = camera.GetPropertyInfo( &propInfo );
	if (error != PGRERROR_OK)
	{
		HandleError( error );
	}
	if ( propInfo.present == true )
	{
		Property prop;
		prop.type = FRAME_RATE;
		error = camera.GetProperty( &prop );
		if (error != PGRERROR_OK)
		{
			HandleError( error );
		}
		option.frameRate = prop.absValue;
	} else {
		option.frameRate = default_fps;
	}
	error = recorder.AVIOpen( video_file_path, &option );
	if (error != PGRERROR_OK)
	{
	   HandleError( error );
	}

	Image image;
	while (true) {
		if(capture) {
			error = camera.RetrieveBuffer( &image );
			if (error != PGRERROR_OK)
			{
				HandleError( error );
				g_print("missed a frame due to error!");
				continue;
			}

			error = recorder.AVIAppend( &image );
			if (error != PGRERROR_OK)
			{
			   HandleError( error );
			}
		} else if (close_avi) {
			g_print("Closing the video file");
			break;
		}
	}

	error = recorder.AVIClose( );
	if (error != PGRERROR_OK)
	{
	   HandleError( error );
	}
	error = camera.StopCapture();
	if (error != PGRERROR_OK)
	{
		HandleError( error );
	}
	error = camera.Disconnect();
	if (error != PGRERROR_OK)
	{
		HandleError( error );
	}

	return NULL;
}

static gboolean close_event( GtkWidget *widget, GdkEvent  *event, gpointer   data )
{
    g_print ("Use the exit button to close the application instead. \n");
    return TRUE; //dont distroy the window
}

/* Another callback */
static void destroy( GtkWidget *widget, gpointer   data )
{
    gtk_main_quit ();
}

/**
 * Stop camera capture
 * */
static void stopCapture( GtkWidget *widget, gpointer   data )
{
	capture = false;
	close_avi = true;
}

/**
 * Main
 * */
int main( int argc, char *argv[] )
{
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box1;
    pthread_t capture_thread;

    g_thread_init(NULL);
    gdk_threads_init();
    gtk_init (&argc, &argv);

    gdk_threads_enter();

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "PGRVideoRecorder", G_CALLBACK (close_event), NULL);
    g_signal_connect (window, "destroy", G_CALLBACK (destroy), NULL);

    gtk_container_set_border_width ( GTK_CONTAINER( window ), 10 );
    box1 = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width( GTK_CONTAINER( window ), 20 );
    gtk_container_add ( GTK_CONTAINER( window ), box1 );

    //Connect
	button = gtk_button_new_with_label ( "CONNECT" );
	g_signal_connect (button, "clicked", G_CALLBACK( connect ), NULL);
	gtk_box_pack_start( GTK_BOX( box1 ), button, TRUE, TRUE, 10 );
	gtk_widget_show(button);

	//Capture
	button = gtk_button_new_with_label ( "Capture" );
	g_signal_connect( button, "clicked", G_CALLBACK( startCapture ), NULL );
	gtk_box_pack_start( GTK_BOX( box1 ), button, TRUE, TRUE, 10 );
	gtk_widget_show( button );

	//Stop
	button = gtk_button_new_with_label ( "Stop" );
	g_signal_connect( button, "clicked", G_CALLBACK( stopCapture ), NULL );
	gtk_box_pack_start( GTK_BOX(box1), button, TRUE, TRUE, 10 );
	gtk_widget_show( button );

    //Exit
    button = gtk_button_new_with_label ("Exit");
    g_signal_connect( button, "clicked", G_CALLBACK( destroy ), window);
    gtk_box_pack_start(GTK_BOX(box1), button, TRUE, TRUE, 10);
    gtk_widget_show (button);

    gtk_widget_show(box1);
    gtk_widget_show (window);

    pthread_create (&capture_thread, NULL, captureFromCamera, NULL);

    gtk_main ();

    gdk_threads_leave();
    return 0;
}
