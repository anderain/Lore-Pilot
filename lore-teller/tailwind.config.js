/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{vue,ts}'],
  theme: {
    extend: {
      fontFamily: {
        mono: ['ui-monospace', 'SFMono-Regular', 'Menlo', 'Consolas', 'monospace'],
      },
      colors: {
        paper: '#f7f2e7',
        'paper-deep': '#efe7d6',
        card: '#fffcf4',
        ink: '#3a352e',
        'ink-soft': '#6f6659',
        'ink-faint': '#9a9184',
        line: '#e2d9c6',
        seal: '#9c3d2e',
        'seal-hover': '#833225',
        'seal-soft': '#f4e4de',
      },
      boxShadow: {
        paper: '0 1px 2px rgba(58, 53, 46, 0.06)',
      },
    },
  },
  plugins: [],
};
