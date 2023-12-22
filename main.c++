#include <gstreamermm.h>
#include <glibmm/main.h>
#include <iostream>
#include <stdlib.h>

Glib::RefPtr<Glib::MainLoop> mainloop;

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

int main(int argc, char** argv)
{
  Gst::init(argc, argv);
  Glib::RefPtr<Gst::Pipeline> pipeline;

  if(argc < 3)
  {
    std::cout << "Usage: " << argv[0] << " <input webm file> <output webm file>" << std::endl;
    return EXIT_FAILURE;
  }

  Glib::RefPtr<Gst::Element> filesrc = Gst::ElementFactory::create_element("filesrc");
  Glib::RefPtr<Gst::Element> filesink = Gst::ElementFactory::create_element("filesink");
  Glib::RefPtr<Gst::Element> demux = Gst::ElementFactory::create_element("matroskademux");
  Glib::RefPtr<Gst::Element> mux = Gst::ElementFactory::create_element("webmmux");
  Glib::RefPtr<Gst::Caps> caps = Gst::Caps::create_simple("video/x-vp9");

  if(!filesrc || !filesink || !demux || !mux)
  {
    std::cerr << "A pipeline element could not be created." << std::endl;
    return EXIT_FAILURE;
  }

  filesrc->set_property<Glib::ustring>("location", argv[1]);
  filesink->set_property<Glib::ustring>("location", argv[2]);
  
  mainloop = Glib::MainLoop::create();
  pipeline = Gst::Pipeline::create("rewriter");

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

		  demux->link(mux, caps);
  });
  mux->link(filesink);
  std::cout << "Running." << std::endl;
  mainloop->run();

  std::cout << "Buffers: " << buffers.size() << std::endl;
  std::cout << "Returned. Setting state to NULL." << std::endl;
  pipeline->set_state(Gst::STATE_NULL);

  return EXIT_SUCCESS;
}
