import React from 'react';
import { 
  LayoutDashboard, 
  MonitorSmartphone, 
  Shield, 
  Settings, 
  Terminal,
  Activity,
  Server
} from 'lucide-react';

const navigationItems = [
  { id: 'dashboard', label: 'Dashboard', icon: LayoutDashboard },
  { id: 'endpoints', label: 'Active Endpoints', icon: MonitorSmartphone },
  { id: 'builder', label: 'Polymorphic Builder', icon: Shield },
  { id: 'settings', label: 'Settings', icon: Settings },
];

function Sidebar({ activeSection, setActiveSection }) {
  return (
    <div className="w-64 bg-zinc-900 border-r border-zinc-800 flex flex-col">
      {/* Logo and Title */}
      <div className="p-6 border-b border-zinc-800">
        <div className="flex items-center space-x-3">
          <div className="w-10 h-10 bg-zinc-800 rounded-lg flex items-center justify-center">
            <Terminal className="w-6 h-6 text-jocky-green" />
          </div>
          <div>
            <h1 className="text-lg font-bold tracking-wider text-white">JOCKY</h1>
            <p className="text-xs text-zinc-500">C2 Framework v2.4.1</p>
          </div>
        </div>
      </div>

      {/* Navigation */}
      <nav className="flex-1 py-4">
        {navigationItems.map((item) => {
          const Icon = item.icon;
          const isActive = activeSection === item.id;
          
          return (
            <button
              key={item.id}
              onClick={() => setActiveSection(item.id)}
              className={`w-full flex items-center space-x-3 px-6 py-3 transition-all duration-200 ${
                isActive
                  ? 'bg-zinc-800 border-l-4 border-jocky-green text-jocky-green'
                  : 'text-zinc-400 hover:bg-zinc-800 hover:text-zinc-200'
              }`}
            >
              <Icon className="w-5 h-5" />
              <span className="text-sm font-medium">{item.label}</span>
              {isActive && (
                <div className="ml-auto w-2 h-2 rounded-full bg-jocky-green animate-pulse" />
              )}
            </button>
          );
        })}
      </nav>

      {/* System Status */}
      <div className="p-4 border-t border-zinc-800">
        <div className="bg-zinc-950 rounded-lg p-4">
          <div className="flex items-center justify-between mb-2">
            <span className="text-xs text-zinc-500">System Status</span>
            <span className="flex items-center">
              <span className="w-2 h-2 bg-jocky-green rounded-full mr-2 animate-pulse" />
              <span className="text-xs text-jocky-green">Operational</span>
            </span>
          </div>
          <div className="space-y-2">
            <div className="flex items-center justify-between">
              <span className="text-xs text-zinc-500">Active Nodes</span>
              <span className="text-xs font-mono text-zinc-300">5</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-xs text-zinc-500">Uptime</span>
              <span className="text-xs font-mono text-zinc-300">23d 14h</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

export default Sidebar;