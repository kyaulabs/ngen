// $KYAULabs: palette-preview.js kyau@aura.kyaulabs 2026/11/15 -0800 Exp $
(() => {
  'use strict';
  const root = document.documentElement;
  const select = document.querySelector('#theme');
  const system = matchMedia('(prefers-color-scheme: dark)');
  function syncTheme() {
    if (select.value === 'system') root.removeAttribute('data-theme');
    else root.dataset.theme = select.value;
    const mode = root.dataset.theme || (system.matches ? 'dark' : 'light');
    document.querySelector('#wordmark').src = '../logos/svg/ngen-wordmark-' + mode + '.svg';
    const style = getComputedStyle(root);
    document.querySelectorAll('[data-token]').forEach((element) => {
      element.textContent = style.getPropertyValue('--ngen-' + element.dataset.token).trim();
    });
    document.querySelectorAll('[data-component]').forEach((element) => {
      element.textContent = style.getPropertyValue('--' + element.dataset.component).trim();
    });
  }
  select.addEventListener('change', syncTheme);
  system.addEventListener('change', syncTheme);
  syncTheme();
})();
// vim: ft=javascript sts=4 sw=4 ts=4 et :
