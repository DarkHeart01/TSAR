/** @type {import('tailwindcss').Config} */
export default {
    content: [
      "./index.html",
      "./src/**/*.{js,ts,jsx,tsx}",
    ],
    theme: {
      extend: {
        colors: {
          'jocky-green': '#00ff9d',
          'jocky-blue': '#00d4ff',
          'jocky-red': '#ff3b3b',
          'jocky-yellow': '#ffd700',
        },
        fontFamily: {
          'mono': ['JetBrains Mono', 'Fira Code', 'monospace'],
        },
        animation: {
          'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
          'blink': 'blink 1s step-end infinite',
        },
        keyframes: {
          blink: {
            '0%, 100%': { opacity: '1' },
            '50%': { opacity: '0' },
          },
        },
      },
    },
    plugins: [],
  }