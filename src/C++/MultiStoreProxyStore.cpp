# include "MultiStoreProxyStore.h"

namespace FIX
{


MessageStore* MultiStoreProxyStoreFactory::create( const SessionID& sessionID )
{
  MultiStoreProxyStore* store = new MultiStoreProxyStore( sessionID );
  return store;
}

void MultiStoreProxyStoreFactory::destroy( MessageStore* store )
{
  delete store;
}


MultiStoreProxyStore::~MultiStoreProxyStore()
{
  for(auto store : m_stores)
  {
    delete store;
  }
  m_stores.clear();
}

bool MultiStoreProxyStore::set( int seqnum, const std::string& message ) throw ( IOException )
{
  bool ok = true;
  for(auto store : m_stores)
  {
    ok = ok && store->set(seqnum, message);
  }
  return ok;
}

void MultiStoreProxyStore::get( int from_seqnum, int to_seqnum, std::vector < std::string > & messages ) const throw ( IOException )
{
  m_stores[0]->get( from_seqnum, to_seqnum, messages );
}


void MultiStoreProxyStore::setNextSenderMsgSeqNum( int value ) throw ( IOException )
{
  for(auto store : m_stores)
  {
    store->setNextSenderMsgSeqNum( value );
  }
}

void MultiStoreProxyStore::setNextTargetMsgSeqNum( int value ) throw ( IOException )
{
  for(auto store : m_stores)
  {
    store->setNextTargetMsgSeqNum( value );
  }
}

void MultiStoreProxyStore::incrNextSenderMsgSeqNum() throw ( IOException )
{
  for(auto store : m_stores)
  {
    store->incrNextSenderMsgSeqNum();
  }
}

void MultiStoreProxyStore::incrNextTargetMsgSeqNum() throw ( IOException )
{
  for(auto store : m_stores)
  {
    store->incrNextTargetMsgSeqNum();
  }
}

void MultiStoreProxyStore::setCreationTime( const UtcTimeStamp& creationTime ) throw ( IOException )
{
  for(auto store : m_stores)
  {
    store->setCreationTime( creationTime );
  }
}

void MultiStoreProxyStore::reset() throw ( IOException )
{
  for(auto store : m_stores)
  {
    store->reset();
  }
}

void MultiStoreProxyStore::refresh() throw ( IOException )
{
  for(auto store : m_stores)
  {
    store->refresh();
  }
}


}
