import React, { useState } from 'react';
import Sidebar from './components/Sidebar';
import Dashboard from './components/Dashboard';

function App() {
  const [activeSection, setActiveSection] = useState('dashboard');

  return (
    <div className="flex h-screen bg-zinc-950 text-zinc-200 overflow-hidden">
      <Sidebar activeSection={activeSection} setActiveSection={setActiveSection} />
      <main className="flex-1 overflow-auto">
        <Dashboard activeSection={activeSection} />
      </main>
    </div>
  );
}

export default App;