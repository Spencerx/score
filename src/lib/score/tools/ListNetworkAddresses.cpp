#include <score/tools/ListNetworkAddresses.hpp>
/*
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/udp.hpp>
*/
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
namespace score
{

QStringList list_ipv4_for_connecting() noexcept
{
#ifndef QT_NO_NETWORKINTERFACE
  QStringList ret;
  for(const QNetworkInterface& iface : QNetworkInterface::allInterfaces())
    for(const QNetworkAddressEntry& entry : iface.addressEntries())
      if(const auto& ip = entry.ip(); ip.protocol() == QAbstractSocket::IPv4Protocol)
        ret.push_back(ip.toString());
  return ret;
#else
  return {"127.0.0.1"};
#endif
}

QStringList list_ipv4_for_listening() noexcept
{
  QStringList ret;
  ret.push_back("0.0.0.0");
  ret << list_ipv4_for_connecting();
  return ret;
  /*
  using namespace boost::asio;
  using resolv = ip::udp::resolver;

  io_context context;

  resolv resolver{context};
  resolv::query query{ip::udp::v4(), ip::host_name(), ""};

  for (auto it = resolver.resolve(query);
       it != resolv::iterator();
       ++it)
  {
    ret.push_back(QString::fromStdString(it->endpoint().address().to_string()));
  }

  if(!ret.contains("127.0.0.1"))
    ret.push_back("127.0.0.1");

  return ret;
  */
}
}
