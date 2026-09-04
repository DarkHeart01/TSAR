import React, { useState } from 'react';
import { Shield, Cpu, Zap, CheckCircle, Loader } from 'lucide-react';
import { buildAPI } from '../api/client';

function BuildPanel({ detailed = false }) {
  const [isBuilding, setIsBuilding] = useState(false);
  const [buildResult, setBuildResult] = useState(null);
  const [buildSteps, setBuildSteps] = useState([]);
  const [error, setError] = useState(null);

  const buildStepsSequence = [
    'Initializing build environment...',
    'Applying LLVM obfuscation...',
    'Injecting anti-analysis routines...',
    'Encrypting payload segments...',
    'Compiling polymorphic shellcode...',
    'Signing with ephemeral certificates...',
    'Deploying to staging server...',
  ];

  const handleBuild = async () => {
    setIsBuilding(true);
    setBuildResult(null);
    setError(null);
    setBuildSteps([]);

    // Simulate build process steps
    for (const step of buildStepsSequence) {
      setBuildSteps(prev => [...prev, step]);
      await new Promise(resolve => setTimeout(resolve, 500 + Math.random() * 1000));
    }

    try {
      const config = {
        target_os: 'windows',
        architecture: 'x64',
        evasion_level: 4,
      };
      
      const result = await buildAPI.buildPayload(config);
      setBuildResult(result);
    } catch (err) {
      setError('Build failed. Check logs for details.');
    } finally {
      setIsBuilding(false);
    }
  };

  return (
    <div className="bg-zinc-900 rounded-lg border border-zinc-800">
      <div className="px-6 py-4 border-b border-zinc-800">
        <h3 className="text-lg font-semibold text-white flex items-center">
          <Shield className="w-5 h-5 mr-2 text-jocky-green" />
          Polymorphic Payload Builder
        </h3>
      </div>
      
      <div className="p-6">
        <div className="flex items-center space-x-4 mb-6">
          <button
            onClick={handleBuild}
            disabled={isBuilding}
            className={`flex items-center space-x-3 px-6 py-3 rounded-lg font-semibold transition-all ${
              isBuilding
                ? 'bg-zinc-800 text-zinc-500 cursor-not-allowed'
                : 'bg-jocky-green text-zinc-900 hover:bg-opacity-90 glow-green'
            }`}
          >
            {isBuilding ? (
              <Loader className="w-5 h-5 animate-spin" />
            ) : (
              <Zap className="w-5 h-5" />
            )}
            <span>
              {isBuilding ? 'Building...' : 'Generate & Deploy Polymorphic Payload'}
            </span>
          </button>
        </div>

        {/* Build Progress */}
        {isBuilding && (
          <div className="bg-zinc-950 rounded-lg p-4 mb-6">
            <div className="font-mono text-sm text-zinc-400 space-y-2">
              {buildSteps.map((step, index) => (
                <div key={index} className="flex items-center space-x-2">
                  <span className="text-jocky-green">→</span>
                  <span>{step}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Build Result */}
        {buildResult && (
          <div className="bg-zinc-950 rounded-lg p-6 border border-jocky-green glow-green">
            <div className="flex items-center space-x-2 mb-4">
              <CheckCircle className="w-5 h-5 text-jocky-green" />
              <span className="text-jocky-green font-semibold">Build Successful</span>
            </div>
            <div className="grid grid-cols-2 gap-4">
              <div>
                <label className="text-xs text-zinc-500 block mb-1">SHA-256 Hash</label>
                <code className="font-mono text-sm text-jocky-blue break-all">
                  {buildResult.sha256}
                </code>
              </div>
              <div>
                <label className="text-xs text-zinc-500 block mb-1">Build ID</label>
                <code className="font-mono text-sm text-zinc-300">
                  {buildResult.build_id}
                </code>
              </div>
              <div>
                <label className="text-xs text-zinc-500 block mb-1">Payload Size</label>
                <span className="text-sm text-zinc-300">{buildResult.size} bytes</span>
              </div>
              <div>
                <label className="text-xs text-zinc-500 block mb-1">Obfuscation</label>
                <span className="text-sm text-zinc-300">{buildResult.obfuscation}</span>
              </div>
            </div>
          </div>
        )}

        {/* Error */}
        {error && (
          <div className="bg-red-950 border border-red-700 rounded-lg p-4">
            <span className="text-red-400">{error}</span>
          </div>
        )}
      </div>
    </div>
  );
}

export default BuildPanel;