import React from 'react';
import EndpointsTable from './EndpointsTable';
import BuildPanel from './BuildPanel';
import TelemetryStream from './TelemetryStream';

function Dashboard({ activeSection }) {
  const renderContent = () => {
    switch (activeSection) {
      case 'dashboard':
        return (
          <div className="space-y-6">
            <DashboardHeader />
            <EndpointsTable />
            <BuildPanel />
            <TelemetryStream />
          </div>
        );
      case 'endpoints':
        return <EndpointsTable detailed />;
      case 'builder':
        return <BuildPanel detailed />;
      case 'settings':
        return <SettingsPanel />;
      default:
        return null;
    }
  };

  return (
    <div className="p-6">
      {renderContent()}
    </div>
  );
}

function DashboardHeader() {
  return (
    <div className="flex items-center justify-between">
      <div>
        <h2 className="text-2xl font-bold text-white">Command & Control Dashboard</h2>
        <p className="text-sm text-zinc-400 mt-1">Real-time operations overview</p>
      </div>
      <div className="flex items-center space-x-4">
        <div className="flex items-center space-x-2 bg-zinc-900 rounded-lg px-4 py-2">
          <div className="w-2 h-2 bg-jocky-green rounded-full animate-pulse" />
          <span className="text-sm text-zinc-300">3 Live Connections</span>
        </div>
        <div className="text-sm text-zinc-500 font-mono">
          UTC: {new Date().toISOString().slice(0, 19).replace('T', ' ')}
        </div>
      </div>
    </div>
  );
}

function SettingsPanel() {
  return (
    <div className="bg-zinc-900 rounded-lg p-6">
      <h3 className="text-lg font-semibold text-white mb-4">Settings</h3>
      <div className="space-y-4">
        <div>
          <label className="block text-sm text-zinc-400 mb-2">C2 Server URL</label>
          <input
            type="text"
            className="w-full bg-zinc-800 rounded-lg px-4 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-jocky-green"
            placeholder="https://c2.jocky.io"
            defaultValue="https://c2.jocky.io"
          />
        </div>
        <div>
          <label className="block text-sm text-zinc-400 mb-2">Encryption Key</label>
          <input
            type="password"
            className="w-full bg-zinc-800 rounded-lg px-4 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-jocky-green"
            placeholder="Enter encryption key"
            defaultValue="AES-256-GCM:************************"
          />
        </div>
      </div>
    </div>
  );
}

export default Dashboard;