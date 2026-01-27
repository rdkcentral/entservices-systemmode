# SystemMode Plugin Architecture Documentation

## Overview

The SystemMode plugin is a WPEFramework (Thunder) plugin designed to provide centralized system mode management and coordination across multiple device components. It enables dynamic system optimization by managing different operational modes and propagating state changes to registered client plugins.

## System Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    WPEFramework (Thunder)                       │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │   Client Plugin │  │   Client Plugin │  │   Client Plugin │  │
│  │ (DisplaySettings)│  │   (Future...)   │  │   (Future...)   │  │
│  └─────────┬───────┘  └─────────┬───────┘  └─────────┬───────┘  │
│            │                    │                    │          │
│            │ IDeviceOptimizeStateActivator Interface │          │
│            └────────────────────┼────────────────────┘          │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                SystemMode Plugin                            │ │
│  │  ┌─────────────────┐  ┌─────────────────────────────────┐  │ │
│  │  │   SystemMode    │  │   SystemModeImplementation      │  │ │
│  │  │   (Plugin)      │  │   (Core Logic)                  │  │ │
│  │  └─────────┬───────┘  └─────────┬───────────────────────┘  │ │
│  │            │                    │                          │ │
│  │            │    ┌───────────────┼────────────────┐         │ │
│  │            │    │               │                │         │ │
│  │  ┌─────────┴────┴─────┐  ┌─────┴────────┐  ┌───┴─────────┐│ │
│  │  │  JSON-RPC Interface│  │State Manager │  │Client Manager││ │
│  │  └────────────────────┘  └──────────────┘  └─────────────┘│ │
│  └─────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                   Utilities & Helpers                           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │ UtilsString.h   │  │ UtilsJsonRpc.h  │  │ UtilsLogging.h  │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

#### 1. SystemMode (Plugin Class)
- **Responsibilities**: Plugin lifecycle management, JSON-RPC interface exposure
- **Interfaces**: `PluginHost::IPlugin`, `PluginHost::JSONRPC`
- **Key Functions**: Initialization, configuration management, API endpoints

#### 2. SystemModeImplementation (Core Logic)
- **Responsibilities**: State management, client coordination, persistence
- **Interfaces**: `Exchange::ISystemMode`
- **Design Pattern**: Singleton pattern for instance management
- **Key Functions**: State transitions, client registration, notification dispatch

#### 3. State Manager
- **Persistence**: File-based storage in `/tmp/SystemMode.txt`
- **Current Modes**: `DEVICE_OPTIMIZE` (VIDEO, GAME states)
- **Default State**: `VIDEO` mode for optimal video playback

#### 4. Client Manager
- **Discovery**: Automatic detection of `IDeviceOptimizeStateActivator` implementations
- **Registration**: Dynamic client plugin registration/unregistration
- **Coordination**: State change propagation to all registered clients

## Data Flow and Interactions

### State Change Flow

```
1. External Request (JSON-RPC)
   ↓
2. SystemMode Plugin (Validation)
   ↓
3. SystemModeImplementation (State Change)
   ↓
4. State Persistence (/tmp/SystemMode.txt)
   ↓
5. Client Notification (All Registered Clients)
   ↓
6. Client State Updates (IDeviceOptimizeStateActivator::Request)
```

### Client Registration Flow

```
1. Client Plugin Activation
   ↓
2. Interface Discovery (IDeviceOptimizeStateActivator)
   ↓
3. Automatic Registration (SystemModeImplementation)
   ↓
4. Current State Notification (New Client)
```

## API Interface Design

### JSON-RPC API
- **Endpoint**: SystemMode plugin callsign
- **Methods**: State request, query, configuration
- **Protocol**: Thunder JSON-RPC standard

### Internal Interfaces
- **ISystemMode**: Core system mode management interface
- **IDeviceOptimizeStateActivator**: Client plugin interface for receiving state changes

## Plugin Framework Integration

### Thunder Integration Points
- **Plugin Loading**: Standard WPEFramework plugin lifecycle
- **Service Discovery**: COM-RPC interface registration
- **Communication**: Inter-plugin communication via Thunder infrastructure
- **Configuration**: Standard Thunder configuration management

### Dependencies
- **Framework**: WPEFramework/Thunder core
- **Utilities**: Custom utility helpers (UtilsString, UtilsLogging, UtilsJsonRpc, UtilsFile)
- **Interfaces**: External interface definitions (Exchange::ISystemMode, IDeviceOptimizeStateActivator)

## Configuration and Deployment

### Build Configuration
- **Plugin Name**: SystemMode
- **Startup Order**: 41 (configurable)
- **Auto-start**: Configurable flag
- **Dependencies**: Thunder plugins, CompileSettingsDebug

### Runtime Configuration
- **State File**: `/tmp/SystemMode.txt`
- **Default Mode**: `DEVICE_OPTIMIZE` in `VIDEO` state
- **Client Discovery**: Automatic via interface introspection

## Technical Implementation Details

### Thread Safety
- **Synchronization**: Mutex-based thread safety for state management
- **Concurrent Access**: Safe multi-client state access and modification

### Performance Considerations
- **State Persistence**: Lightweight file-based approach for fast access
- **Client Notification**: Asynchronous notification to prevent blocking
- **Memory Management**: Efficient client list management with dynamic registration

### Error Handling
- **State Validation**: Input validation for state change requests
- **Client Communication**: Graceful handling of client communication failures
- **File I/O**: Robust file operation error handling with fallback defaults

## Extensibility Points

### Future Enhancements
- **Additional System Modes**: Framework supports multiple system modes beyond DEVICE_OPTIMIZE
- **New States**: Easy addition of new states within existing modes
- **Enhanced Persistence**: Database backend for improved persistence and querying
- **Advanced Coordination**: Complex state transition logic and dependencies

### Integration Opportunities
- **Platform Services**: Integration with power management, thermal management
- **Application Services**: Coordination with media players, gaming engines
- **System Monitoring**: Integration with performance monitoring and telemetry