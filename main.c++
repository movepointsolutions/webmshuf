#include <gstreamermm.h>
#include <glibmm/main.h>
#include <iostream>
#include <stdlib.h>

Glib::RefPtr<Glib::MainLoop> mainloop;
Glib::RefPtr<Gst::Caps> stream_caps;

// Taken from tutorials
static gboolean print_field (GQuark field, const GValue * value, gpointer pfx) {
  gchar *str = gst_value_serialize (value);

  g_print ("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string (field), str);
  g_free (str);
  return TRUE;
}


static void print_caps (const GstCaps * caps, const gchar * pfx) {
  guint i;

  g_return_if_fail (caps != NULL);

  if (gst_caps_is_any (caps)) {
    g_print ("%sANY\n", pfx);
    return;
  }
  if (gst_caps_is_empty (caps)) {
    g_print ("%sEMPTY\n", pfx);
    return;
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);

    g_print ("%s%s\n", pfx, gst_structure_get_name (structure));
    gst_structure_foreach (structure, print_field, (gpointer) pfx);
  }
}

bool on_bus_message(const Glib::RefPtr<Gst::Bus>& /* bus */,
  const Glib::RefPtr<Gst::Message>& message)
{
  switch(message->get_message_type()) {
    case Gst::MESSAGE_EOS:
      std::cout << std::endl << "End of stream" << std::endl;
      mainloop->quit();
      return false;
    case Gst::MESSAGE_ERROR:
    {
      Glib::RefPtr<Gst::MessageError> msgError =
        Glib::RefPtr<Gst::MessageError>::cast_static(message);

      if(msgError)
      {
        Glib::Error err;
	std::string debug;
        msgError->parse(err, debug);
        std::cerr << "Error: " << err.what() << std::endl;
	std::cerr << "Debugging info: " << debug << std::endl;
      }
      else
        std::cerr << "Error." << std::endl;

        mainloop->quit();
        return false;
    }
    default:
      break;
  }

  return true;
}

static int n_probes = 0;
std::vector<Glib::RefPtr<Gst::Buffer>> buffers;

Gst::PadProbeReturn der_probe(const Glib::RefPtr<Gst::Pad> &pad, const Gst::PadProbeInfo &probe_info)
{
	n_probes++;
	Glib::RefPtr<Gst::Buffer> buf = probe_info.get_buffer();
	//std::cout << "DTS: " << buf->get_dts() << std::endl;
	//std::cout << "PTS: " << buf->get_pts() << std::endl;
	buffers.push_back(buf->copy());
	return Gst::PAD_PROBE_OK;
}

int collect_buffers(const char *filename)
{
  Glib::RefPtr<Gst::Pipeline> pipeline;
  Glib::RefPtr<Gst::Element> filesrc = Gst::ElementFactory::create_element("filesrc");
  Glib::RefPtr<Gst::Element> filesink = Gst::ElementFactory::create_element("fakesink");
  Glib::RefPtr<Gst::Element> demux = Gst::ElementFactory::create_element("matroskademux");
  Glib::RefPtr<Gst::Element> mux = Gst::ElementFactory::create_element("webmmux");
  Glib::RefPtr<Gst::Caps> caps = Gst::Caps::create_simple("video/x-vp9");

  if(!filesrc || !filesink || !demux || !mux)
  {
    std::cerr << "A pipeline element could not be created." << std::endl;
    return EXIT_FAILURE;
  }

  filesrc->set_property<Glib::ustring>("location", filename);
  //filesink->set_property<Glib::ustring>("location", argv[2]);
  
  pipeline = Gst::Pipeline::create("collect_buffers");

  Glib::RefPtr<Gst::Bus> bus = pipeline->get_bus();
  bus->add_watch(sigc::ptr_fun(&on_bus_message));

  pipeline->add(filesrc)->add(demux)->add(mux)->add(filesink);

  std::cout << "Setting to PLAYING." << std::endl;
  pipeline->set_state(Gst::STATE_PLAYING);
  filesrc->link(demux);
  demux->signal_pad_added().connect([demux, mux, caps] (const Glib::RefPtr<Gst::Pad>& pad) {
		  std::cout << "New pad added to " << demux->get_name() << std::endl;
		  std::cout << "Pad name: " << pad->get_name() << std::endl;
		  pad->add_probe(Gst::PAD_PROBE_TYPE_BUFFER, sigc::ptr_fun(der_probe));

		  demux->link(mux);
		  //demux->link(mux, caps);
		  stream_caps = pad->get_current_caps();
  });
  mux->link(filesink);

  std::cout << "Running." << std::endl;
  mainloop->run();

  std::cout << "Buffers: " << buffers.size() << std::endl;
  std::cout << "Returned. Setting state to NULL." << std::endl;
  pipeline->set_state(Gst::STATE_NULL);
  return 0;
}

int write_file(const char *filename)
{
  Glib::RefPtr<Gst::Pipeline> pipeline;
  Glib::RefPtr<Gst::AppSrc> appsrc = Gst::AppSrc::create();
  Glib::RefPtr<Gst::Element> filesink = Gst::ElementFactory::create_element("filesink");
  Glib::RefPtr<Gst::Element> mux = Gst::ElementFactory::create_element("webmmux");

  if(!appsrc || !filesink || !mux)
  {
    std::cerr << "A pipeline element could not be created." << std::endl;
    return EXIT_FAILURE;
  }

  filesink->set_property<Glib::ustring>("location", filename);
  
  pipeline = Gst::Pipeline::create("write_file");

  Glib::RefPtr<Gst::Bus> bus = pipeline->get_bus();
  bus->add_watch(sigc::ptr_fun(&on_bus_message));

  pipeline->add(appsrc)->add(mux)->add(filesink);

  print_caps(stream_caps->gobj(), ":::");
  appsrc->property_is_live().set_value(true);
  appsrc->property_format().set_value(Gst::FORMAT_TIME);
  appsrc->set_caps(stream_caps);
  auto apppad = appsrc->get_static_pad("src");
  //apppad->set_caps(stream_caps);
  apppad->use_fixed_caps();
  appsrc->signal_pad_added().connect([appsrc] (const Glib::RefPtr<Gst::Pad>& pad) {
	  std::cout << "New pad added to " << appsrc->get_name() << std::endl;
	  std::cout << "Pad name: " << pad->get_name() << std::endl;
	  print_caps(pad->get_current_caps()->gobj(), "PAD|");
  });
  mux->signal_pad_added().connect([mux, filesink] (const Glib::RefPtr<Gst::Pad>& pad) {
	  std::cout << "New pad added to " << mux->get_name() << std::endl;
	  std::cout << "Pad name: " << pad->get_name() << std::endl;
  });
  appsrc->link(mux);
  mux->link(filesink);
  sigc::connection feed_connection;
  auto feed = [appsrc] () {
	  std::cout << "Feed buffer" << std::endl;
	  static int i = 0;
	  auto buf = buffers[i++]->copy();
	  static guint pts = 0;
	  buf->set_pts(pts);
	  pts += buf->get_duration();
	  appsrc->push_buffer(buf);
	  return true;
  };
  appsrc->signal_need_data().connect([appsrc, feed, &feed_connection] (guint size) {
		  std::cout << "Need data" << std::endl;
	feed_connection = Glib::signal_idle().connect(feed);
  });
  appsrc->signal_enough_data().connect([appsrc, &feed_connection] () {
		  std::cout << "Enough data" << std::endl;
	feed_connection.disconnect();
  });

  std::cout << "Setting to PLAYING." << std::endl;
  pipeline->set_state(Gst::STATE_PLAYING);

  std::cout << "Running." << std::endl;
  mainloop->run();

  std::cout << "Returned. Setting state to NULL." << std::endl;
  pipeline->set_state(Gst::STATE_NULL);
  return 0;
}

int main(int argc, char** argv)
{
  Gst::init(argc, argv);

  if(argc < 3)
  {
    std::cout << "Usage: " << argv[0] << " <input webm file> <output webm file>" << std::endl;
    return EXIT_FAILURE;
  }
  mainloop = Glib::MainLoop::create();

  collect_buffers(argv[1]);
  write_file(argv[2]);

  return EXIT_SUCCESS;
}
