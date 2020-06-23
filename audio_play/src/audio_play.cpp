#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <ros/ros.h>
#include <boost/thread.hpp>


//new code start -----------------
#include <sensor_msgs/Joy.h>
#include <std_msgs/Bool.h>
#include <ros/console.h>

#include <memory>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
//new code end -------------------

#include "audio_common_msgs/AudioData.h"

namespace audio_transport
{
  class RosGstPlay
  {
    public:
      RosGstPlay()
      {
        GstPad *audiopad;

        //Set button pressed to FALSE
        button_is_pressed = false;


        std::string dst_type;
        std::string device;
        bool do_timestamp;

        // The destination of the audio
        ros::param::param<std::string>("~dst", dst_type, "alsasink");
        ros::param::param<std::string>("~device", device, std::string());
        ros::param::param<bool>("~do_timestamp", do_timestamp, true);

        _sub = _nh.subscribe("audio", 10, &RosGstPlay::onAudio, this);

        //Initialize subscriber that listens to joy topic to see if button
        //is pressed 
        button_sub = _nh.subscribe("/joy", 10, &RosGstPlay::JoyCallback, this);

        _loop = g_main_loop_new(NULL, false);

        _pipeline = gst_pipeline_new("app_pipeline");
        _source = gst_element_factory_make("appsrc", "app_source");
        g_object_set(G_OBJECT(_source), "do-timestamp", (do_timestamp) ? TRUE : FALSE, NULL);
        gst_bin_add( GST_BIN(_pipeline), _source);

        //_playbin = gst_element_factory_make("playbin2", "uri_play");
        //g_object_set( G_OBJECT(_playbin), "uri", "file:///home/test/test.mp3", NULL);
        if (dst_type == "alsasink")
        {
          _decoder = gst_element_factory_make("decodebin", "decoder");
          g_signal_connect(_decoder, "pad-added", G_CALLBACK(cb_newpad),this);
          gst_bin_add( GST_BIN(_pipeline), _decoder);
          gst_element_link(_source, _decoder);

          _audio = gst_bin_new("audiobin");
          _convert = gst_element_factory_make("audioconvert", "convert");
          audiopad = gst_element_get_static_pad(_convert, "sink");
          _sink = gst_element_factory_make("autoaudiosink", "sink");
          if (!device.empty()) {
            g_object_set(G_OBJECT(_sink), "device", device.c_str(), NULL);
          }
          gst_bin_add_many( GST_BIN(_audio), _convert, _sink, NULL);
          gst_element_link(_convert, _sink);
          gst_element_add_pad(_audio, gst_ghost_pad_new("sink", audiopad));
          gst_object_unref(audiopad);

          gst_bin_add(GST_BIN(_pipeline), _audio);
        }
        else
        {
          _sink = gst_element_factory_make("filesink", "sink");
          g_object_set( G_OBJECT(_sink), "location", dst_type.c_str(), NULL);
          gst_bin_add(GST_BIN(_pipeline), _sink);
          gst_element_link(_source, _sink);
        }

        gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_PLAYING);
        //gst_element_set_state(GST_ELEMENT(_playbin), GST_STATE_PLAYING);

        _gst_thread = boost::thread( boost::bind(g_main_loop_run, _loop) );
        ROS_INFO_STREAM("_gst_thread");
      }

      //Callback function when button is pressed
      void JoyCallback(const sensor_msgs::Joy::ConstPtr& joy){

        //if button was pressed and is now released, re-initialize loop
        if(button_is_pressed && !(joy->buttons[2])){
          ROS_INFO_STREAM("before run");
          init_loop();
          ROS_INFO_STREAM("after run");
        }
        //if button was released and is now pressed quit audio output
        else if(!button_is_pressed && (joy->buttons[2])){
          ROS_INFO_STREAM("before quit");
          g_main_loop_quit(_loop);
          g_main_loop_unref(_loop);
          ROS_INFO_STREAM("after quit");
        }
        //set button_is_pressed bool
        button_is_pressed = joy->buttons[2];
        ROS_INFO_STREAM("button_is_pressed: " << button_is_pressed);


      }

    private:

      //Copied code from constructor to use to re-initialize loop when
      //button is released
      void init_loop(){

        GstPad *audiopad;

        std::string dst_type;
        std::string device;
        bool do_timestamp;

        // The destination of the audio
        ros::param::param<std::string>("~dst", dst_type, "alsasink");
        ros::param::param<std::string>("~device", device, std::string());
        ros::param::param<bool>("~do_timestamp", do_timestamp, true);

        _loop = g_main_loop_new(NULL, false);

        _pipeline = gst_pipeline_new("app_pipeline");
        _source = gst_element_factory_make("appsrc", "app_source");
        g_object_set(G_OBJECT(_source), "do-timestamp", (do_timestamp) ? TRUE : FALSE, NULL);
        gst_bin_add( GST_BIN(_pipeline), _source);

        //_playbin = gst_element_factory_make("playbin2", "uri_play");
        //g_object_set( G_OBJECT(_playbin), "uri", "file:///home/test/test.mp3", NULL);
        if (dst_type == "alsasink")
        {
          _decoder = gst_element_factory_make("decodebin", "decoder");
          g_signal_connect(_decoder, "pad-added", G_CALLBACK(cb_newpad),this);
          gst_bin_add( GST_BIN(_pipeline), _decoder);
          gst_element_link(_source, _decoder);

          _audio = gst_bin_new("audiobin");
          _convert = gst_element_factory_make("audioconvert", "convert");
          audiopad = gst_element_get_static_pad(_convert, "sink");
          _sink = gst_element_factory_make("autoaudiosink", "sink");
          if (!device.empty()) {
            g_object_set(G_OBJECT(_sink), "device", device.c_str(), NULL);
          }
          gst_bin_add_many( GST_BIN(_audio), _convert, _sink, NULL);
          gst_element_link(_convert, _sink);
          gst_element_add_pad(_audio, gst_ghost_pad_new("sink", audiopad));
          gst_object_unref(audiopad);

          gst_bin_add(GST_BIN(_pipeline), _audio);
        }
        else
        {
          _sink = gst_element_factory_make("filesink", "sink");
          g_object_set( G_OBJECT(_sink), "location", dst_type.c_str(), NULL);
          gst_bin_add(GST_BIN(_pipeline), _sink);
          gst_element_link(_source, _sink);
        }

        gst_element_set_state(GST_ELEMENT(_pipeline), GST_STATE_PLAYING);
        //gst_element_set_state(GST_ELEMENT(_playbin), GST_STATE_PLAYING);

        _gst_thread = boost::thread( boost::bind(g_main_loop_run, _loop) );
        ROS_INFO_STREAM("_gst_thread");
      }

      //Callback function when audio msg is received
      void onAudio(const audio_common_msgs::AudioDataConstPtr &msg)
      {
        if(!button_is_pressed){

          GstBuffer *buffer = gst_buffer_new_and_alloc(msg->data.size());
          gst_buffer_fill(buffer, 0, &msg->data[0], msg->data.size());
          GstFlowReturn ret;

          g_signal_emit_by_name(_source, "push-buffer", buffer, &ret);
          ROS_INFO_STREAM("Audio Ausgefuehrt" << msg->data[0]);
        }

      }

     static void cb_newpad (GstElement *decodebin, GstPad *pad,
                             gpointer data)
      {
        RosGstPlay *client = reinterpret_cast<RosGstPlay*>(data);

        GstCaps *caps;
        GstStructure *str;
        GstPad *audiopad;

        /* only link once */
        audiopad = gst_element_get_static_pad (client->_audio, "sink");
        if (GST_PAD_IS_LINKED (audiopad))
        {
          g_object_unref (audiopad);
          return;
        }

        /* check media type */
        caps = gst_pad_query_caps (pad, NULL);
        str = gst_caps_get_structure (caps, 0);
        if (!g_strrstr (gst_structure_get_name (str), "audio")) {
          gst_caps_unref (caps);
          gst_object_unref (audiopad);
          return;
        }

        gst_caps_unref (caps);

        /* link'n'play */
        gst_pad_link (pad, audiopad);

        g_object_unref (audiopad);
        ROS_INFO_STREAM("cb_newpad ausgefuehrt");
      }

      ros::NodeHandle _nh;
      ros::Subscriber _sub;
      boost::thread _gst_thread;

      GstElement *_pipeline, *_source, *_sink, *_decoder, *_convert, *_audio;
      GstElement *_playbin;
      GMainLoop *_loop;

      //New code start ---------------
      bool button_is_pressed;
      ros::Subscriber button_sub;
      //New code end ---------------------
  };
}


int main (int argc, char **argv)
{
  ros::init(argc, argv, "audio_play");
  gst_init(&argc, &argv);

  audio_transport::RosGstPlay client;

  ros::spin();
}
