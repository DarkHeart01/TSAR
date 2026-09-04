import React, { useEffect, useState } from 'react';
import { MonitorSmartphone, Cpu, Network, Clock, MoreVertical } from 'lucide-react';
import { endpointsAPI } from '../api/client';
import StatusIndicator from './StatusIndicator';

function EndpointsTable({ detailed = false }) {
  const [endpoints, setEndpoints] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    fetchEndpoints();
    const interval = setInterval(fetchEndpoints, 30000);
    return () => clearInterval(interval);
  }, []);

  const fetchEndpoints = async () => {
    try {
      const data = await endpointsAPI.getEndpoints();
      setEndpoints(data.endpoints);
      setLoading(false);
    } catch (err) {
      setError('Failed to fetch endpoints');
      setLoading(false);
    }
  };

  const formatLastSeen = (timestamp) => {
    const seconds = Math.floor((Date.now() - new Date(timestamp).getTime()) / 1000);
    if (seconds < 60) return `${seconds}s ago`;
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
    return `${Math.floor(seconds / 86400)}d ago`;
  };

  if (loading) {
    return (
      <div className="bg-zinc-900 rounded-lg p-6 animate-pulse">
        <div className="h-4 bg-zinc-800 rounded w-1/4 mb-4" />
        <div className="space-y-3">
          {[...Array(3)].map((_, i) => (
            <div key={i} className="h-12 bg-zinc-800 rounded" />
          ))}
        </div>
      </div>
    );
  }

  return (
    <div className="bg-zinc-900 rounded-lg overflow-hidden border border-zinc-800">
      <div className="px-6 py-4 border-b border-zinc-800 flex items-center justify-between">
        <h3 className="text-lg font-semibold text-white flex items-center">
          <MonitorSmartphone className="w-5 h-5 mr-2 text-jocky-green" />
          Active Endpoints
        </h3>
        <span className="text-sm text-zinc-400">
          {endpoints.filter(ep => ep.status === 'live').length} live / {endpoints.length} total
        </span>
      </div>
      
      <div className="overflow-x-auto">
        <table className="w-full">
          <thead className="bg-zinc-950">
            <tr>
              <th className="px-6 py-3 text-left text-xs font-medium text-zinc-400 uppercase tracking-wider">Endpoint ID</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-zinc-400 uppercase tracking-wider">Hostname & OS</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-zinc-400 uppercase tracking-wider">Internal IP</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-zinc-400 uppercase tracking-wider">Status</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-zinc-400 uppercase tracking-wider">Last Seen</th>
              <th className="px-6 py-3 text-left text-xs font-medium text-zinc-400 uppercase tracking-wider">Actions</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-zinc-800">
            {endpoints.map((endpoint) => (
              <tr key={endpoint.id} className="hover:bg-zinc-800 transition-colors">
                <td className="px-6 py-4">
                  <span className="font-mono text-sm text-jocky-blue">{endpoint.id}</span>
                </td>
                <td className="px-6 py-4">
                  <div className="text-sm font-medium text-white">{endpoint.hostname}</div>
                  <div className="text-xs text-zinc-400 flex items-center mt-1">
                    <Cpu className="w-3 h-3 mr-1" />
                    {endpoint.os}
                  </div>
                </td>
                <td className="px-6 py-4">
                  <span className="font-mono text-sm text-zinc-300 flex items-center">
                    <Network className="w-4 h-4 mr-2 text-zinc-500" />
                    {endpoint.internal_ip}
                  </span>
                </td>
                <td className="px-6 py-4">
                  <StatusIndicator status={endpoint.status} />
                </td>
                <td className="px-6 py-4">
                  <span className="text-sm text-zinc-400 flex items-center">
                    <Clock className="w-4 h-4 mr-2 text-zinc-500" />
                    {formatLastSeen(endpoint.last_seen)}
                  </span>
                </td>
                <td className="px-6 py-4">
                  <button className="text-zinc-400 hover:text-white transition-colors">
                    <MoreVertical className="w-5 h-5" />
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

export default EndpointsTable;