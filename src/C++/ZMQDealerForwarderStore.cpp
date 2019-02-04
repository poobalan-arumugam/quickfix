#include <vector>
#include <string>
#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include "ZMQDealerForwarderStore.h"

namespace FIX
{

typedef std::vector<std::string> zmq_frames_t;

bool zmq_recv_frames(zmq::socket_t& socket, zmq_frames_t& frames)
{
    while(true){
        zmq::message_t message;
        socket.recv(&message);

        int more = 0;
        size_t more_size = sizeof(more);
        socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);

        std::string data(static_cast<char*>(message.data()), message.size());
        frames.push_back(data);

        if(!more){
            break;
        }
    }
    return frames.size() > 0;
}

void zmq_send_frames(zmq::socket_t& socket, const zmq_frames_t& frames)
{
    int remainder = frames.size();
    for(auto& frame: frames){
        --remainder;
        zmq::message_t message(frame.data(), frame.size());
        int more = (remainder > 0) ? ZMQ_SNDMORE : 0;
        socket.send(message, more);
    }
}


std::string uuid_string()
{
  boost::uuids::uuid uuid1;
  std::string sv = boost::lexical_cast<std::string>(uuid1);
  //std::stringstream ostream;
  //ostream << uuid1;
  //std::string sv1 = ostream.str();
  return sv;
}


void zmq_request(zmq::socket_t& socket,
		 const std::string& request_counter_string,
		 const std::string& request_id,
		 const std::string& attempt_count_string,
		 const std::string& command,
		 const std::string& sessionid_string,
		 const zmq_frames_t& request)
{
  zmq_frames_t frames;
  frames.push_back(request_counter_string);
  frames.push_back(request_id);
  frames.push_back(attempt_count_string);
  frames.push_back(command);
  frames.push_back(sessionid_string);
  frames.insert(std::end(frames),
		std::begin(request), std::end(request));
  zmq_send_frames(socket, frames);
}


void zmq_request(zmq::socket_t& socket,
		 uint64_t& request_counter,
		 const std::string& request_id,
		 int attempt_count,
		 const std::string& command,
		 const std::string& sessionid_string,
		 const zmq_frames_t& request)
{
  std::string request_counter_string = "UNKNOWN";
  {
    std::ostringstream ostream;
    ostream << request_counter << std::flush;
    request_counter_string = ostream.str();
  }
  std::string attempt_count_string = "UNKNOWN";
  {
    std::ostringstream ostream;
    ostream << attempt_count << std::flush;
    attempt_count_string = ostream.str();
  }

  zmq_request(socket,
	      request_counter_string,
	      request_id,
	      attempt_count_string,
	      command,
	      sessionid_string,
	      request);
}


bool zmq_reply(zmq::socket_t& socket,
	       const uint64_t& request_counter,
           const std::string& request_id,
           zmq_frames_t& response_frames)
{
  int local_retries = 10;
  while(local_retries-- > 0)
  {
    zmq_recv_frames(socket, response_frames);
    if(response_frames.size() < 3)
    {
      continue;
    }

    if(response_frames.size() > 3 and response_frames[1] == request_id)
    {
      return true;
    }

    {
      std::istringstream istream(response_frames[0]);
      uint64_t response_request_counter = 0;
      istream >> response_request_counter;
      if(response_request_counter < request_counter)
      {
        continue;
      }
    }
  }
  return false;
}


bool zmq_request_reply(zmq::socket_t& socket,
		       const std::string& command,
		       const std::string& sessionid_string,
		       const zmq_frames_t& request,
		       zmq_frames_t& response_frames,
		       uint64_t& request_counter,
		       int max_retry_count = 3)
{
  request_counter += 1;
  int attempt_count = 0;
  std::string request_id =  uuid_string();
  while(attempt_count++ < max_retry_count)
  {
    zmq_request(socket,
		request_counter, request_id, attempt_count,
		command, sessionid_string, request);

    return zmq_reply(socket,
                     request_counter,
                     request_id,
                     response_frames);
  }
  return false;
}


bool zmq_request_reply(zmq::socket_t& socket,
		       const std::string& command,
		       const std::string& sessionid_string,
		       zmq_frames_t& response_frames,
		       uint64_t& request_counter,
		       int max_retry_count = 3)
{
  zmq_frames_t request;
  return zmq_request_reply(socket,
			   command,
			   sessionid_string,
			   request,
			   response_frames,
			   request_counter,
			   max_retry_count);
}


ZMQDealerForwarderStore::~ZMQDealerForwarderStore()
{
  m_socket.close();
  m_zmq_context.close();
}

MessageStore* ZMQDealerForwarderStoreFactory::create( const SessionID& sessionID )
{
    Dictionary settings = m_settings.get();
    std::string url = settings.getString( ZMQ_MESSAGE_STORE_URL );
    ZMQDealerForwarderStore* store = new ZMQDealerForwarderStore(sessionID, url);
    return store;
}

ZMQDealerForwarderStore::ZMQDealerForwarderStore( const SessionID& sessionID, const std::string& url ) :
  m_sessionID( sessionID ), m_url( url ), m_sessionid_string(),
  m_nextSenderMsgSeqNum( 1 ), m_nextTargetMsgSeqNum( 1 ),
  m_zmq_context(), m_socket( m_zmq_context, ZMQ_DEALER ),
  m_request_counter()
{
  m_socket.connect(url);
  {
    std::ostringstream ostream;
    ostream << m_sessionID << std::flush;
    m_sessionid_string = ostream.str();
  }

  zmq_frames_t response_frames;
  zmq_request_reply(m_socket, "initialise", m_sessionid_string,
		    response_frames, m_request_counter);
}


void ZMQDealerForwarderStoreFactory::destroy( MessageStore* store )
{
    delete store;
}


void sendSetValue(zmq::socket_t& socket,
                  uint64_t& request_counter,
                  const std::string& command,
                  const std::string& sessionid_string,
                  const std::string& sv)
{
    zmq_frames_t request;
    request.push_back(sv);

    request_counter += 1;
    zmq_request(socket,
        request_counter,
        "", // request_id
        0,  // attempt_count
        command,
        sessionid_string,
        request);
}

void sendSetValue(zmq::socket_t& socket,
                  uint64_t& request_counter,
                  const std::string& command,
                  const std::string& sessionid_string,
                  const int seqnum)
{
    std::string sv;
    {
      std::ostringstream ostream;
      ostream << seqnum << std::flush;
      sv = ostream.str();
    }

    sendSetValue(socket,
                 request_counter,
                 command,
                 sessionid_string,
                 sv);
}

void ZMQDealerForwarderStore::setNextSenderMsgSeqNum( int value ) throw ( IOException )
{
    m_nextSenderMsgSeqNum = value;
    sendSetValue(m_socket,
                 m_request_counter,
                 "setNextSenderMsgSeqNum",
                 m_sessionid_string,
                 value);
}

void ZMQDealerForwarderStore::setNextTargetMsgSeqNum( int value ) throw ( IOException )
{
    m_nextTargetMsgSeqNum = value;
    sendSetValue(m_socket,
                 m_request_counter,
                 "setNextTargetMsgSeqNum",
                 m_sessionid_string,
                 value);
}

void ZMQDealerForwarderStore::incrNextSenderMsgSeqNum() throw ( IOException )
{
    ++m_nextSenderMsgSeqNum;
    sendSetValue(m_socket,
                 m_request_counter,
                 "incrNextSenderMsgSeqNum",
                 m_sessionid_string,
                 m_nextSenderMsgSeqNum);
}

void ZMQDealerForwarderStore::incrNextTargetMsgSeqNum() throw ( IOException )
{
    ++m_nextTargetMsgSeqNum;
    sendSetValue(m_socket,
                 m_request_counter,
                 "incrNextTargetMsgSeqNum",
                 m_sessionid_string,
                 m_nextTargetMsgSeqNum);
}

void ZMQDealerForwarderStore::setCreationTime( const UtcTimeStamp& creationTime ) throw ( IOException )
{
    m_creationTime = creationTime;

    std::string sv = UtcTimeStampConvertor::convert( m_creationTime );
    sendSetValue(m_socket,
                 m_request_counter,
                 "setCreationTime",
                 m_sessionid_string,
                 sv);
}


bool ZMQDealerForwarderStore::set( int seqnum, const std::string& message ) throw ( IOException )
{
    std::string sv;
    {
      std::ostringstream ostream;
      ostream << seqnum << std::flush;
      sv = ostream.str();
    }

    zmq_frames_t request;
    request.push_back(sv);
    request.push_back(message);

    m_request_counter += 1;
    zmq_request(m_socket,
        m_request_counter,
        "", // request_id
        0,  // attempt_count
        "set",
        m_sessionid_string,
		request);

    return true;
}

void ZMQDealerForwarderStore::get( int from_seqnum, int to_seqnum, std::vector < std::string > & messages ) const throw ( IOException )
{
    std::string sv;
    {
      std::ostringstream ostream;
      ostream << from_seqnum << "," << to_seqnum << std::flush;
      sv = ostream.str();
    }

    zmq_frames_t request_frames;
    request_frames.push_back(sv);
    zmq_frames_t response_frames;
    zmq_request_reply(const_cast<zmq::socket_t&>(m_socket), "get", m_sessionid_string,
		      request_frames, response_frames,
		      const_cast<uint64_t&>(m_request_counter));

    if(response_frames.size() > 0)
    {
        auto from_iter = std::begin(response_frames);
        std::advance(from_iter, request_frames.size());
        messages.insert(std::end(messages),
                        from_iter,
                        std::end(response_frames));
    }
}


void ZMQDealerForwarderStore::reset() throw ( IOException )
{
  try
  {
    std::string sv;
    {
      m_nextSenderMsgSeqNum = 1;
      m_nextTargetMsgSeqNum = 1;
      m_creationTime.setCurrent();

      sv = UtcTimeStampConvertor::convert( m_creationTime );
      std::ostringstream ostream;
      ostream << m_nextSenderMsgSeqNum
	      << " "
	      << m_nextTargetMsgSeqNum
	      << " "
	      << sv
	      << std::flush;
      sv = ostream.str();
    }

    zmq_frames_t request_frames;
    request_frames.push_back(sv);
    zmq_frames_t response_frames;
    zmq_request_reply(m_socket, "reset-request", m_sessionid_string,
		      request_frames, response_frames,
		      m_request_counter);

    if(response_frames.size() > 0)
    {
    }
  }
  catch( std::exception& e )
  {
    throw IOException( e.what() );
  }
}

void ZMQDealerForwarderStore::refresh() throw ( IOException )
{
  try
  {
    zmq_frames_t request_frames;
    //request_frames.push_back(sv);
    zmq_frames_t response_frames;
    zmq_request_reply(m_socket, "refresh-request", m_sessionid_string,
		      request_frames, response_frames,
		      m_request_counter);

    if(response_frames.size() > 0)
    {
      {
	std::string sv = response_frames[response_frames.size() - 1];
	std::istringstream istream(sv);
	istream >> m_nextSenderMsgSeqNum
		>> m_nextTargetMsgSeqNum
		>> sv;
	m_creationTime = UtcTimeStampConvertor::convert( sv );
      }
    }

  }
  catch( std::exception& e )
  {
    throw IOException( e.what() );
  }
}


void test()
{
    zmq::context_t ctx;
    zmq::socket_t socket(ctx, ZMQ_DEALER);
    socket.connect("tcp://127.0.0.1:4200");

    zmq_frames_t frames;
    zmq_recv_frames(socket, frames);
    zmq_send_frames(socket, frames);
}

}
