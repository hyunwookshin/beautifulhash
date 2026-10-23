// Shared by every BeautifulHash page: the light/dark theme switch.
"use strict";

(() => {
  // Theme: system (the default), light, or dark, remembered per browser.
  const THEME_KEY = "beautifulhash.theme";
  const THEMES = ["system", "light", "dark"];
  const ICONS = {
    system: '<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M8 20h8M12 16v4"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
    dark: '<path d="M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z"/>',
  };
  const applyTheme = (name) => {
    if (name === "system") delete document.documentElement.dataset.theme;
    else document.documentElement.dataset.theme = name;
    document.getElementById("theme").innerHTML = `<svg viewBox="0 0 24 24" width="18" height="18" aria-hidden="true">${ICONS[name]}</svg>`;
    const label = `Theme: ${name}`;
    document.getElementById("theme").setAttribute("aria-label", label);
    document.getElementById("theme").title = label;
  };
  applyTheme(document.documentElement.dataset.theme || "system");
  document.getElementById("theme").addEventListener("click", () => {
    const next = THEMES[(THEMES.indexOf(document.documentElement.dataset.theme || "system") + 1) % THEMES.length];
    applyTheme(next);
    try { next === "system" ? localStorage.removeItem(THEME_KEY) : localStorage.setItem(THEME_KEY, next); } catch { /* storage unavailable */ }
  });
})();
