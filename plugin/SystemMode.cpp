/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "SystemMode.h"
#include "SystemModeImplementation.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::SystemMode> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin
    {

    /*
     *Register SystemMode module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(SystemMode, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    SystemMode::SystemMode()
        : _service(nullptr)
        , _connectionId(0)
        , _systemMode(nullptr)
        , _pluginNotification(*this)
    {
        SYSLOG(Logging::Startup, (_T("SystemMode Constructor")));
    }

    SystemMode::~SystemMode()
    {
        SYSLOG(Logging::Shutdown, (string(_T("SystemMode Destructor"))));
    }

    const string SystemMode::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _systemMode);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("SystemMode::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _systemMode = _service->Root<Exchange::ISystemMode>(_connectionId, 5000, _T("SystemModeImplementation"));

        if(nullptr != _systemMode)
        {
            Exchange::JSystemMode::Register(*this, _systemMode);
            
            // Register for plugin state notifications (in-process handler)
            SYSLOG(Logging::Startup, (_T("SystemMode::Initialize: Registering for plugin state notifications")));
            service->Register(&_pluginNotification);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("SystemMode::Initialize: Failed to initialise SystemMode plugin")));
            message = _T("SystemMode plugin could not be initialised");
        }

        if (0 != message.length())
        {
           Deinitialize(service);
        }

        return message;
    }

    void SystemMode::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("SystemMode::Deinitialize"))));

        if (nullptr != _systemMode)
        {
            // Unregister plugin state notifications
            SYSLOG(Logging::Shutdown, (_T("SystemMode::Deinitialize: Unregistering plugin state notifications")));
            service->Unregister(&_pluginNotification);
            
            Exchange::JSystemMode::Unregister(*this);

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _systemMode->Release();

            _systemMode = nullptr;
	    ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // If this was running in a (container) process...
            if (nullptr != connection)
            {
               // Lets trigger the cleanup sequence for
               // out-of-process code. Which will guard
               // that unwilling processes, get shot if
               // not stopped friendly :-)
               connection->Terminate();
               connection->Release();
            }
        }

        _connectionId = 0;
        _service->Release();
        _service = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("SystemMode de-initialised"))));
    }

    string SystemMode::Information() const
    {
       // No additional info to report
       return (string());
    }

    void SystemMode::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {
            ASSERT(nullptr != _service);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

    // PluginStateNotification implementation
    void SystemMode::PluginStateNotification::Activated(const string& callsign, PluginHost::IShell* shell)
    {
        _parent.OnPluginActivated(callsign, shell);
    }

    void SystemMode::PluginStateNotification::Deactivated(const string& callsign, PluginHost::IShell* /* shell */)
    {
        _parent.OnPluginDeactivated(callsign);
    }

    void SystemMode::PluginStateNotification::Unavailable(const string& callsign, PluginHost::IShell* /* shell */)
    {
        // Plugin unavailable - no action needed
    }

    // Plugin lifecycle event handlers
    void SystemMode::OnPluginActivated(const string& callsign, PluginHost::IShell* shell)
    {
        LOGDBG("Plugin activated: %s - checking for IDeviceOptimizeStateActivator interface", callsign.c_str());
        
        if (shell == nullptr) {
            LOGWARN("OnPluginActivated: shell is nullptr for callsign %s", callsign.c_str());
            return;
        }
        
        // Query the activated plugin for IDeviceOptimizeStateActivator interface
        auto deviceOptimize = shell->QueryInterface<Exchange::IDeviceOptimizeStateActivator>();
        
        if (deviceOptimize != nullptr) {
            // Plugin implements the interface - register via RPC interface
            LOGDBG("Plugin %s implements IDeviceOptimizeStateActivator - registering via ISystemMode", callsign.c_str());
            
            // Release our local reference - SystemModeImplementation will manage it
            deviceOptimize->Release();
            
            // Use the RPC interface to register (works for in-process or out-of-process)
            if (_systemMode != nullptr) {
                _systemMode->ClientActivated(callsign, "DEVICE_OPTIMIZE");
                LOGINFO("Plugin %s successfully registered as DeviceOptimize client", callsign.c_str());
            }
        } else {
            // Plugin doesn't implement the interface - ignore
            LOGDBG("Plugin %s does not implement IDeviceOptimizeStateActivator - ignoring", callsign.c_str());
        }
    }

    void SystemMode::OnPluginDeactivated(const string& callsign)
    {
        LOGDBG("Plugin deactivated: %s - notifying SystemMode", callsign.c_str());
        
        // Use the RPC interface to deregister (works for in-process or out-of-process)
        if (_systemMode != nullptr) {
            _systemMode->ClientDeactivated(callsign, "DEVICE_OPTIMIZE");
            LOGINFO("Plugin %s deactivation notification sent to SystemMode", callsign.c_str());
        }
    }

} // namespace Plugin
} // namespace WPEFramework
