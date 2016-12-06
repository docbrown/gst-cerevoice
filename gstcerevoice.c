/* CereVoice Text-to-Speech Plugin for GStreamer
 * Copyright (C) 2016 David Brown <cypher543@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <cerevoice_eng.h>

static CPRCEN_engine *global_engine = NULL;
G_LOCK_DEFINE_STATIC(global_engine);

static gboolean init_global_engine() {
  gboolean ret = TRUE;
  if (g_once_init_enter(&global_engine)) {
    CPRCEN_engine *e = CPRCEN_engine_new();
    if (!e)
    ret = FALSE;
    g_once_init_leave(&global_engine, e);
  }
  return ret;
}

GST_DEBUG_CATEGORY_STATIC(gst_cerevoice_debug);
#define GST_CAT_DEFAULT gst_cerevoice_debug

#define GST_TYPE_CEREVOICE gst_cerevoice_get_type()
G_DECLARE_FINAL_TYPE(GstCereVoice, gst_cerevoice, GST, CEREVOICE, GstElement)

struct _GstCereVoice {
  GstElement             parent_instance;
  GstPad                *sinkpad;
  GstPad                *srcpad;
  gchar                 *voice_name;
  gchar                 *voice_file;
  gchar                 *license_file;
  gchar                 *config_file;
  CPRCEN_channel_handle  chan;
  int                    rate;
  GstEvent              *segment_event;
};

struct _GstCereVoiceClass {
  GstElementClass parent_class;
};

G_DEFINE_TYPE(GstCereVoice, gst_cerevoice, GST_TYPE_ELEMENT);

enum {
  PROP_0,
  PROP_VOICE_NAME,
  PROP_VOICE_FILE,
  PROP_LICENSE_FILE,
  PROP_CONFIG_FILE
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
  "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
  GST_STATIC_CAPS("text/x-raw, format=(string)utf-8")
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
  "src", GST_PAD_SRC, GST_PAD_ALWAYS,
  GST_STATIC_CAPS("audio/x-raw, format=(string)" GST_AUDIO_NE(S16) ", "
    "channels=(int)1, rate=(int)[1, 2147483647], "
    "layout=(string)interleaved")
);

static void
gst_cerevoice_callback(CPRC_abuf *abuf, void *userdata)
{
  GstCereVoice *self = GST_CEREVOICE(userdata);
  GstBuffer *out;
  short *data = CPRC_abuf_wav_data(abuf);
  int samples = CPRC_abuf_wav_sz(abuf);
  int start = MAX(CPRC_abuf_wav_mk(abuf), 0);
  int end = MAX(CPRC_abuf_wav_done(abuf), 0);
  gsize size = (end - start) * sizeof(*data);
  
  GST_LOG_OBJECT(self, "got audio buffer, %d samples "
    "(safe region: %d-%d, %d samples, %lu bytes)",
    samples, start, end, end - start, size);

  out = gst_buffer_new_allocate(NULL, size, NULL);
  GST_BUFFER_TIMESTAMP(out) = GST_CLOCK_TIME_NONE;
  gst_buffer_fill(out, 0, data + start, size);
  gst_pad_push(self->srcpad, out);
}

static gboolean
gst_cerevoice_open_channel(GstCereVoice *self)
{
  gboolean ret = TRUE;
  CPRCEN_channel_handle chan = 0;
  int rate;

  if (self->voice_file) {
    /* Acquire a lock to ensure other threads can't load a voice until we've
     * created our channel */
    G_LOCK(global_engine);

    if (!CPRCEN_engine_load_voice(global_engine,
      self->license_file, self->config_file, self->voice_file,
      CPRC_VOICE_LOAD))
    {
      GST_ELEMENT_ERROR(GST_ELEMENT(self), RESOURCE, FAILED,
        ("Failed to load voice from %s.", self->voice_file), (NULL));
      goto fail;
    }
    
    /* A default channel will use the voice we just loaded */
    chan = CPRCEN_engine_open_default_channel(global_engine);
    
    G_UNLOCK(global_engine);
  } else {
    /* No voice specified, so we just use whatever was loaded last */
    chan = CPRCEN_engine_open_default_channel(global_engine);
  }
  
  if (G_UNLIKELY(!chan)) {
    GST_ELEMENT_ERROR(GST_ELEMENT(self), LIBRARY, FAILED,
      ("Failed to open synthesizer channel."), (NULL));
    goto fail;
  }
  
  if (!CPRCEN_engine_set_callback(global_engine, chan, self,
    gst_cerevoice_callback))
  {
    GST_ELEMENT_ERROR(GST_ELEMENT(self), LIBRARY, FAILED,
      ("Failed to set synthesizer channel callback."), (NULL));
    goto fail;
  }
  
  rate = atoi(CPRCEN_channel_get_voice_info(
      global_engine, chan, "SAMPLE_RATE"));
  if (rate == 0) {
    GST_ELEMENT_ERROR(GST_ELEMENT(self), LIBRARY, FAILED,
      ("Failed to get voice sample rate."), (NULL));
    goto fail;
  }
  
  self->chan = chan;
  self->rate = rate;
  goto cleanup;

fail:
  if (chan)
    CPRCEN_engine_channel_close(global_engine, chan);
  ret = FALSE;

cleanup:
  return ret;
}

static void
gst_cerevoice_close_channel(GstCereVoice *self)
{
  CPRCEN_engine_channel_close(global_engine, self->chan);
  self->chan = 0;
}

static GstStateChangeReturn
gst_cerevoice_change_state(GstElement *element, GstStateChange transition)
{
  GstCereVoice *self = GST_CEREVOICE(element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  
  GST_DEBUG_OBJECT(self, "changing state: %s => %s",
    gst_element_state_get_name(GST_STATE_TRANSITION_CURRENT(transition)),
    gst_element_state_get_name(GST_STATE_TRANSITION_NEXT(transition)));
  
  switch (transition) {
  case GST_STATE_CHANGE_NULL_TO_READY:
    if (!init_global_engine())
      return GST_STATE_CHANGE_FAILURE;
    break;
  case GST_STATE_CHANGE_READY_TO_PAUSED:
    if (!gst_cerevoice_open_channel(self))
      return GST_STATE_CHANGE_FAILURE;
    break;
  default: break;
  }
  
  ret = GST_ELEMENT_CLASS(gst_cerevoice_parent_class)->change_state(
    element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;
  
  switch (transition) {
  case GST_STATE_CHANGE_READY_TO_NULL:
    break;
  case GST_STATE_CHANGE_PAUSED_TO_READY:
    gst_cerevoice_close_channel(self);
    break;
  default: break;
  }
  
  return ret;
}

static void
gst_cerevoice_set_property(GObject *obj, guint prop,
  const GValue *val, GParamSpec *spec)
{
  GstCereVoice *self = GST_CEREVOICE(obj);
  switch (prop) {
  case PROP_VOICE_NAME:
    g_free(self->voice_name);
    self->voice_name = g_value_dup_string(val);
    break;
  case PROP_VOICE_FILE:
    g_free(self->voice_file);
    self->voice_file = g_value_dup_string(val);
    break;
  case PROP_LICENSE_FILE:
    g_free(self->license_file);
    self->license_file = g_value_dup_string(val);
    break;
  case PROP_CONFIG_FILE:
    g_free(self->config_file);
    self->config_file = g_value_dup_string(val);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, spec);
    break;
  }
}

static void
gst_cerevoice_get_property(GObject *obj, guint prop,
  GValue *val, GParamSpec *spec)
{
  GstCereVoice *self = GST_CEREVOICE(obj);
  switch (prop) {
  case PROP_VOICE_NAME:   g_value_set_string(val, self->voice_name);   break;
  case PROP_VOICE_FILE:   g_value_set_string(val, self->voice_file);   break;
  case PROP_LICENSE_FILE: g_value_set_string(val, self->license_file); break;
  case PROP_CONFIG_FILE:  g_value_set_string(val, self->config_file);  break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, spec);
    break;
  }
}

static GstFlowReturn
gst_cerevoice_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
  GstCereVoice *self = GST_CEREVOICE(parent);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo info;
  
  GST_LOG_OBJECT(self, "got text buffer, %" G_GSIZE_FORMAT " bytes",
    gst_buffer_get_size(buf));
  
  if (!gst_pad_has_current_caps(self->srcpad)) {
    GstCaps *caps;
    
    GST_DEBUG_OBJECT(self, "negotiating sample rate to %d Hz", self->rate);
    
    caps = gst_caps_new_simple("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE(S16),
      "channels", G_TYPE_INT, 1,
      "rate", G_TYPE_INT, self->rate,
      "layout", G_TYPE_STRING, "interleaved", NULL);
    if (!gst_pad_set_caps(self->srcpad, caps)) {
      GST_ELEMENT_ERROR(self, CORE, NEGOTIATION, (NULL), (NULL));
      goto failed;
    }
    
    /* Now that caps have been negotiated, we can go ahead and push a
       pending segment event, if we have one. */
    if (self->segment_event) {
      gst_pad_push_event(self->srcpad, self->segment_event);
      self->segment_event = NULL;
    }
  }
  
  gst_buffer_map(buf, &info, GST_MAP_READ);
  
  if (!CPRCEN_engine_channel_speak(
      global_engine, self->chan, (const char *)info.data, info.size, 1))
  {
    GST_ERROR_OBJECT(self, "failed to speak text buffer");
    ret = GST_FLOW_ERROR;
  }
 
failed:
  gst_buffer_unmap(buf, &info);
  gst_buffer_unref(buf);
  return ret;
}

static gboolean
gst_cerevoice_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
  GstCereVoice *self = GST_CEREVOICE(parent);
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_SEGMENT:
    /* Segment events should only be forwarded after caps negotiation. */
    if (gst_pad_has_current_caps(self->srcpad)) {
      /* Source pad has caps, so pass the event on through. */
      ret = gst_pad_event_default(pad, parent, event);
    } else {
      /* Caps haven't been negotiated, so just save the segment event. It
         will be sent by the source pad chain function. */
      gst_event_replace(&self->segment_event, event);
      gst_event_unref(event);
    }
    break;
  default:
    ret = gst_pad_event_default(pad, parent, event);
    break;
  }

  return ret;
}

static void
gst_cerevoice_class_init(GstCereVoiceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  
  gobject_class->set_property = gst_cerevoice_set_property;
  gobject_class->get_property = gst_cerevoice_get_property;
  
  g_object_class_install_property(gobject_class, PROP_VOICE_NAME,
    g_param_spec_string("voice-name", "Voice name",
      "Set/Get voice name", "", G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class, PROP_VOICE_FILE,
    g_param_spec_string("voice-file", "Voice file",
      "Set/Get voice file", "", G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class, PROP_LICENSE_FILE,
    g_param_spec_string("license-file", "License file",
      "Set/Get license file", "", G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class, PROP_CONFIG_FILE,
    g_param_spec_string("config-file", "Configuration file",
      "Set/Get configuration file", "", G_PARAM_READWRITE));
  
  element_class->change_state = GST_DEBUG_FUNCPTR(gst_cerevoice_change_state);
  
  gst_element_class_set_static_metadata(element_class,
    "CereVoice Text-to-Speech Synthesizer",
    "Filter/Effect/Audio",
    "Converts plain text and SSML to audio",
    "David Brown <cypher543@gmail.com>");
  
  gst_element_class_add_static_pad_template(element_class, &sink_factory);
  gst_element_class_add_static_pad_template(element_class, &src_factory);
}

static void
gst_cerevoice_init(GstCereVoice *self)
{
  self->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
  gst_pad_set_chain_function(self->sinkpad,
    GST_DEBUG_FUNCPTR(gst_cerevoice_chain));
  gst_pad_set_event_function(self->sinkpad,
    GST_DEBUG_FUNCPTR(gst_cerevoice_sink_event));
  gst_element_add_pad(GST_ELEMENT(self), self->sinkpad);
  
  self->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
  gst_pad_use_fixed_caps(self->srcpad);
  gst_element_add_pad(GST_ELEMENT(self), self->srcpad);
}

static gboolean
plugin_init(GstPlugin *plugin)
{
  GST_DEBUG_CATEGORY_INIT(gst_cerevoice_debug,
    "cerevoice", 0, "cerevoice tts element");
  return gst_element_register(plugin,
    "cerevoice", GST_RANK_NONE, GST_TYPE_CEREVOICE);
}

#define PACKAGE "gstcerevoice"
GST_PLUGIN_DEFINE(
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  cerevoice,
  "CereVoice Text-to-Speech Plugin",
  plugin_init,
  "0.1",
  "LGPL",
  "David Brown",
  "http://github.com/docbrown/gst-cerevoice"
)

