import React, { useEffect, useRef, useState } from 'react';
import { Terminal } from 'lucide-react';
import { telemetryAPI } from '../api/client';

function TelemetryStream() {
  const [events, setEvents] = useState([]);
  const [isLive, setIsLive] = useState(true);
  const terminalRef = useRef(null);

  useEffect(() => {
    fetchTelemetry();
    const interval = setInterval(fetchTelemetry, 5000);
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    if (terminalRef.current) {
      terminalRef.current.scrollTop = terminalRef.current.scrollHeight;
    }
  }, [events]);

  const fetchTelemetry = async () => {
    try {
      const data = await telemetryAPI.getTelemetry();
      setEvents(data.events);
      setIsLive(data.stream_active);
    } catch (error) {
      console.error('Failed to fetch telemetry:', error);
      setIsLive(false);
    }
  };

  const getLevelColor = (level) => {
    switch (level.toUpperCase()) {
      case 'CRIT':
        return 'text-jocky-red';
      case 'WARN':
        return 'text-jocky-yellow';
      default:
        return 'text-jocky-green';
    }
  };

  return (
    <div className="bg-zinc-900 rounded-lg border border-zinc-800">
      <div className="px-6 py-4 border-b border-zinc-800 flex items-center justify-between">
        <h3 className="text-lg font-semibold text-white flex items-center">
          <Terminal className="w-5 h-5 mr-2 text-jocky-green" />
          Live Telemetry Stream
        </h3>
        <div className="flex items-center space-x-2">
          {isLive ? (
            <>
              <span className="w-2 h-2 bg-jocky-green rounded-full animate-pulse" />
              <span className="text-sm text-jocky-green">Streaming</span>
            </>
          ) : (
            <>
              <span className="w-2 h-2 bg-jocky-red rounded-full" />
              <span className="text-sm text-jocky-red">Disconnected</span>
            </>
          )}
        </div>
      </div>
      
      <div
        ref={terminalRef}
        className="h-64 overflow-y-auto p-4 bg-zinc-950 font-mono text-sm"
      >
        {events.map((event, index) => (
          <div key={index} className="mb-2 flex items-start space-x-3">
            <span className="text-zinc-600 shrink-0">
              [{new Date(event.timestamp).toLocaleTimeString()}]
            </span>
            <span className="text-zinc-500 shrink-0">[{event.endpoint}]</span>
            <span className={getLevelColor(event.level)}>
              {event.message}
            </span>
          </div>
        ))}
        {isLive && (
          <div className="flex items-center space-x-2 mt-2">
            <span className="text-jocky-green animate-pulse">▊</span>
            <span className="text-zinc-600">Waiting for incoming data...</span>
          </div>
        )}
      </div>
    </div>
  );
}

export default TelemetryStream;