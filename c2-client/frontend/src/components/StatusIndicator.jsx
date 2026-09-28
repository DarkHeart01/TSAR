import React from 'react';

function StatusIndicator({ status }) {
  const isLive = status === 'live';
  
  return (
    <div className="flex items-center space-x-2">
      <div className={`w-2 h-2 rounded-full ${
        isLive ? 'bg-jocky-green animate-pulse' : 'bg-zinc-600'
      }`} />
      <span className={`text-sm ${
        isLive ? 'text-jocky-green' : 'text-zinc-500'
      }`}>
        {isLive ? 'Live' : 'Offline'}
      </span>
    </div>
  );
}

export default StatusIndicator;