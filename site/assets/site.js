(() => {
  "use strict";

  /* ---------- Theme toggle ----------
     With no saved override, CSS and this script follow the operating-system
     preference. The button switches to the opposite theme; pressing it again
     returns to automatic mode. */
  const root = document.documentElement;
  const systemTheme = window.matchMedia("(prefers-color-scheme: dark)");
  let themeMode = root.dataset.theme === "dark" || root.dataset.theme === "light"
    ? root.dataset.theme
    : "system";
  const currentTheme = () => themeMode === "system"
    ? (systemTheme.matches ? "dark" : "light")
    : themeMode;

  const syncThemeButton = (button) => {
    const dark = currentTheme() === "dark";
    const automatic = themeMode === "system";
    button.setAttribute("aria-pressed", automatic ? "false" : "true");
    button.setAttribute(
      "aria-label",
      automatic
        ? `Following system ${dark ? "dark" : "light"} theme; switch to ${dark ? "light" : "dark"}`
        : `Using ${dark ? "dark" : "light"} theme; return to system theme`
    );
    button.title = automatic ? "Theme: system" : `Theme: ${dark ? "dark" : "light"} (click for system)`;
  };

  const applyTheme = () => {
    if (themeMode === "system") delete root.dataset.theme;
    else root.dataset.theme = themeMode;
    const dark = currentTheme() === "dark";
    document.querySelectorAll('meta[name="theme-color"]').forEach((meta) => {
      meta.content = dark ? "#000000" : "#f7f9ff";
    });
    document.querySelectorAll(".theme-toggle").forEach(syncThemeButton);
  };

  document.querySelectorAll(".theme-toggle").forEach((button) => {
    button.addEventListener("click", () => {
      themeMode = themeMode === "system"
        ? (currentTheme() === "dark" ? "light" : "dark")
        : "system";
      try {
        if (themeMode === "system") localStorage.removeItem("mwfl-theme");
        else localStorage.setItem("mwfl-theme", themeMode);
      } catch { /* private mode */ }
      applyTheme();
    });
  });
  systemTheme.addEventListener("change", () => {
    if (themeMode === "system") applyTheme();
  });
  applyTheme();

  /* ---------- Mobile navigation ---------- */
  document.querySelectorAll(".nav").forEach((nav) => {
    const links = nav.querySelector(".nav-links");
    if (!links) return;

    if (!links.id) links.id = "main-navigation";
    const toggle = document.createElement("button");
    toggle.type = "button";
    toggle.className = "nav-menu-toggle";
    toggle.setAttribute("aria-controls", links.id);
    toggle.setAttribute("aria-expanded", "false");
    toggle.textContent = "Menu";
    nav.insertBefore(toggle, links);

    const themeToggle = links.querySelector(".theme-toggle");
    const mobileNavigation = window.matchMedia("(max-width: 640px)");
    const placeThemeToggle = () => {
      if (!themeToggle) return;
      if (mobileNavigation.matches) nav.insertBefore(themeToggle, links);
      else links.append(themeToggle);
    };
    placeThemeToggle();

    const closeMenu = () => {
      links.classList.remove("nav-links-open");
      toggle.setAttribute("aria-expanded", "false");
    };
    toggle.addEventListener("click", () => {
      const open = links.classList.toggle("nav-links-open");
      toggle.setAttribute("aria-expanded", String(open));
    });
    links.addEventListener("click", (event) => {
      if (event.target.closest("a")) closeMenu();
    });
    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        closeMenu();
        toggle.focus();
      }
    });
    mobileNavigation.addEventListener("change", (event) => {
      placeThemeToggle();
      if (!event.matches) closeMenu();
    });
  });

  /* ---------- Window chrome + copy button ----------
     Every standalone code block becomes a small "native window": caption bar
     with a title, a copy button, and Windows caption glyphs. Blocks inside
     component cards stay plain. Without JavaScript, <pre> styling stands alone. */
  const defaultTitle = (code) => {
    const text = code.textContent;
    if (/^\s*(#|\$|cmake |git |ctest )/m.test(text) && !text.includes("#include")) {
      return /cmake_minimum_required|CMakeLists/.test(text) ? "CMakeLists.txt" : "PowerShell";
    }
    if (/^\s*[{[]/.test(text)) return "settings.json";
    return "main.cpp";
  };

  document.querySelectorAll("pre").forEach((pre) => {
    if (pre.closest(".component-card, .window")) return;
    const code = pre.querySelector("code");
    const title = pre.dataset.title || (code ? defaultTitle(code) : "main.cpp");

    const frame = document.createElement("figure");
    frame.className = "window";
    const bar = document.createElement("div");
    bar.className = "window-bar";

    const label = document.createElement("span");
    label.className = "window-title";
    label.textContent = title;

    const copy = document.createElement("button");
    copy.type = "button";
    copy.className = "copy-btn";
    copy.textContent = "Copy";
    copy.setAttribute("aria-label", "Copy code to clipboard");
    copy.addEventListener("click", async () => {
      try {
        await navigator.clipboard.writeText(pre.textContent.trimEnd());
        copy.textContent = "Copied ✓";
        copy.classList.add("copied");
        setTimeout(() => {
          copy.textContent = "Copy";
          copy.classList.remove("copied");
        }, 1600);
      } catch {
        copy.textContent = "Press Ctrl+C";
      }
    });

    const glyphs = document.createElement("span");
    glyphs.className = "window-glyphs";
    glyphs.setAttribute("aria-hidden", "true");
    glyphs.textContent = "─ □ ✕";

    bar.append(label, copy, glyphs);
    pre.replaceWith(frame);
    frame.append(bar, pre);
  });

  /* ---------- Lightweight C++ syntax highlighting ---------- */
  const keywords = new Set([
    "alignas", "auto", "bool", "break", "case", "catch", "class", "const",
    "constexpr", "continue", "default", "delete", "do", "else", "explicit",
    "false", "final", "for", "if", "namespace", "new", "noexcept", "nullptr",
    "operator", "override", "private", "protected", "public", "requires", "return",
    "static", "struct", "template", "this", "throw", "true", "try", "typename",
    "using", "virtual", "void", "while"
  ]);
  const types = new Set([
    "DWORD", "HINSTANCE", "HRESULT", "HWND", "LRESULT", "PWSTR", "UINT", "WPARAM",
    "Application", "Button", "CommandEvent", "ControlId", "EventResult", "ImageList",
    "KeyEvent", "Label", "ListView", "MainWindow", "RectDip", "TaskDialogResult",
    "TimerId", "TreeView", "UiTimer", "WindowBase"
  ]);
  const tokenPattern = /(?<!:)\/\/[^\n]*|\/\*[\s\S]*?\*\/|L?"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|^\s*#[^\n]*|\b(?:[A-Za-z_]\w*|\d+(?:\.\d+)?)\b/gm;

  document.querySelectorAll("pre code").forEach((code) => {
    const source = code.textContent;
    const fragment = document.createDocumentFragment();
    let cursor = 0;
    for (const match of source.matchAll(tokenPattern)) {
      fragment.append(document.createTextNode(source.slice(cursor, match.index)));
      const token = match[0];
      let kind = "";
      if (token.startsWith("//") || token.startsWith("/*")) kind = "comment";
      else if (token.trimStart().startsWith("#")) kind = "preprocessor";
      else if (/^L?["']/.test(token)) kind = "string";
      else if (/^\d/.test(token)) kind = "number";
      else if (keywords.has(token)) kind = "keyword";
      else if (types.has(token) || /^[A-Z][A-Za-z0-9_]*$/.test(token)) kind = "type";

      if (kind) {
        const span = document.createElement("span");
        span.className = `syntax-${kind}`;
        span.textContent = token;
        fragment.append(span);
      } else {
        fragment.append(document.createTextNode(token));
      }
      cursor = match.index + token.length;
    }
    fragment.append(document.createTextNode(source.slice(cursor)));
    code.replaceChildren(fragment);
  });

  /* ---------- Complete component catalog ---------- */
  const catalogSearch = document.querySelector("#component-search");
  if (catalogSearch) {
    const cards = [...document.querySelectorAll(".capability-card")];
    const groups = [...document.querySelectorAll(".capability-group")];
    const filters = [...document.querySelectorAll("[data-catalog-filter]")];
    const result = document.querySelector(".catalog-result");
    let category = "all";

    const applyCatalogFilter = () => {
      const query = catalogSearch.value.trim().toLocaleLowerCase();
      let visible = 0;
      cards.forEach((card) => {
        const categoryMatch = category === "all" || card.dataset.category === category;
        const queryMatch = !query || card.dataset.search.includes(query);
        card.hidden = !(categoryMatch && queryMatch);
        if (!card.hidden) visible += 1;
      });
      groups.forEach((group) => {
        group.hidden = !group.querySelector(".capability-card:not([hidden])");
      });
      result.textContent = `Showing ${visible} ${visible === 1 ? "capability" : "capabilities"}.`;
    };

    catalogSearch.addEventListener("input", applyCatalogFilter);
    filters.forEach((button) => {
      button.addEventListener("click", () => {
        category = button.dataset.catalogFilter;
        filters.forEach((candidate) => {
          const selected = candidate === button;
          candidate.classList.toggle("active", selected);
          candidate.setAttribute("aria-pressed", String(selected));
        });
        applyCatalogFilter();
      });
      button.setAttribute("aria-pressed", String(button.classList.contains("active")));
    });
  }

  /* ---------- Example image viewer ---------- */
  const exampleCards = document.querySelectorAll(".example-card");
  if (exampleCards.length) {
    const dialog = document.createElement("dialog");
    dialog.className = "example-viewer";
    dialog.setAttribute("aria-labelledby", "example-viewer-title");
    dialog.innerHTML = `
      <div class="example-viewer-frame">
        <header class="example-viewer-bar">
          <div><strong id="example-viewer-title"></strong><span class="example-viewer-description"></span></div>
          <button class="example-viewer-close" type="button" aria-label="Close preview">&times;</button>
        </header>
        <div class="example-viewer-canvas"><img alt=""></div>
        <footer class="example-viewer-actions">
          <span>Native screenshot</span>
          <a class="button primary example-viewer-source" target="_blank" rel="noopener noreferrer">Source code <span aria-hidden="true">↗</span></a>
        </footer>
      </div>`;
    document.body.append(dialog);

    const title = dialog.querySelector("#example-viewer-title");
    const description = dialog.querySelector(".example-viewer-description");
    const preview = dialog.querySelector("img");
    const source = dialog.querySelector(".example-viewer-source");
    const close = dialog.querySelector(".example-viewer-close");

    exampleCards.forEach((card) => {
      card.setAttribute("aria-haspopup", "dialog");
      card.addEventListener("click", (event) => {
        event.preventDefault();
        const image = card.querySelector("img");
        title.textContent = card.querySelector("strong").textContent;
        description.textContent = card.querySelector("span").textContent;
        preview.src = image.currentSrc || image.src;
        preview.alt = image.alt;
        source.href = card.href;
        dialog.showModal();
      });
    });

    close.addEventListener("click", () => dialog.close());
    dialog.addEventListener("click", (event) => {
      if (event.target === dialog) dialog.close();
    });
  }
})();
