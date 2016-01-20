#include "simppl/stub.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"

#include <cstring>
#include <sstream>


namespace simppl
{

namespace ipc
{


StubBase::StubBase(const char* iface, const char* role)
 : role_(role)
 , conn_state_(ConnectionState::Disconnected)
 , disp_(0)
{
   assert(iface);
   assert(role);

   // strip template arguments
   memset(iface_, 0, sizeof(iface_));
   strncpy(iface_, iface, strstr(iface, "<") - iface);

   // remove '::' separation in favour of '.' separation
   char *readp = iface_, *writep = iface_;
   while(*readp)
   {
      if (*readp == ':')
      {
         *writep++ = '.';
         readp += 2;
      }
      else
         *writep++ = *readp++;
   }

   // terminate
   *writep = '\0';
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->removeClient(*this);
}


DBusConnection* StubBase::conn()
{
    return disp().conn_;
}


Dispatcher& StubBase::disp()
{
   assert(disp_);
   return *disp_;
}


std::string StubBase::boundname() const
{
    std::ostringstream bname;
    bname << iface_ << "." << role_;

    return bname.str();
}


void StubBase::connection_state_changed(ConnectionState state)
{
   std::cout << "1I" << std::endl;
   conn_state_ = state;

std::cout << "2I" << std::endl;
   if (connected)
   {
      std::cout << "3I" << std::endl;
      connected(conn_state_);
      std::cout << "4I" << std::endl;
   }
}


void StubBase::sendSignalRegistration(ClientSignalBase& sigbase)
{
   assert(disp_);

   DBusError err;
   dbus_error_init(&err);

   // FIXME do we really need dependency to dispatcher?
   std::ostringstream match_string;
   match_string << "type='signal'";
   match_string << ",sender='" << boundname() << "'";
   match_string << ",interface='" << iface() << "'";
   match_string << ",member='" << sigbase.name() << "'";

   dbus_bus_add_match(disp_->conn_, match_string.str().c_str(), &err);
   //assert(!dbus_error_is_set(&err));
   if (dbus_error_is_set(&err))
       std::cerr << err.name << " " << err.message << std::endl;

   // FIXME handle double attach
   signals_[sigbase.name()] = &sigbase;

   dbus_error_free(&err);
}


void StubBase::getProperty(const char* name, void(*callback)(DBusPendingCall*, void*), void* user_data)
{
   DBusMessage* msg = dbus_message_new_method_call(boundname().c_str(), objectpath().c_str(), "org.freedesktop.DBus.Properties", "Get");
   DBusPendingCall* pending = nullptr;

   detail::Serializer s(msg);
   s << iface();
   s << name;

   dbus_connection_send_with_reply(conn(), msg, &pending, -1);
   dbus_message_unref(msg);

   dbus_pending_call_set_notify(pending, callback, user_data, 0);
}


DBusHandlerResult StubBase::try_handle_signal(DBusMessage* msg)
{
   const char *method = dbus_message_get_member(msg);

   auto iter = signals_.find(dbus_message_get_member(msg));
   if (iter != signals_.end())
   {
       iter->second->eval(msg);
       return DBUS_HANDLER_RESULT_HANDLED;
   }
   else
      std::cout << "Signal " << dbus_message_get_member(msg) << " not found" << std::endl;

   return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


void StubBase::sendSignalUnregistration(ClientSignalBase& sigbase)
{
   assert(disp_);

   DBusError err;
   dbus_error_init(&err);

   std::ostringstream match_string;
   match_string << "type='signal'";
   match_string << ", sender='" << role() << "'";
   match_string << ", interface='" << iface() << "'";
   match_string << ", member='" << sigbase.name() << "'";

   dbus_bus_remove_match(disp_->conn_, match_string.str().c_str(), &err);
   assert(!dbus_error_is_set(&err));

   dbus_error_free(&err);
   // FIXME remove signalbase handler again
}


}   // namespace ipc

}   // namespace simppl

