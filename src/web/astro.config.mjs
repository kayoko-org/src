import { defineConfig } from 'astro/config';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  vite: {
    plugins: [tailwindcss()],
  },
  markdown: {
    shikiConfig: {
      // 'slack-white' or 'min-light' are very minimal and avoid heavy colors
      theme: 'min-light',
    },
  },
});
