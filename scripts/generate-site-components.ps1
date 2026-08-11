[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

function Encode-Html([string]$Value) {
    return [System.Net.WebUtility]::HtmlEncode($Value)
}

function Display-Name([string]$Id) {
    $special = @{
        'application-window' = 'Application and Window'
        'sdi-notepad-reference' = 'SDI Notepad Reference'
        'direct2d-drawing' = 'Direct2D Drawing'
        'direct3d-swap-chain' = 'Direct3D Swap Chain'
        'webview2-browser' = 'WebView2 Browser'
        'pdf-viewer' = 'PDF Viewer'
        'scintilla-code-editor' = 'Scintilla Code Editor'
        'markdown-editor' = 'Markdown Editor'
        'windows-ribbon' = 'Windows Ribbon'
        'legacy-mdi' = 'Legacy MDI'
        'emf-gdiplus' = 'EMF and GDI+'
        'taskbar-help' = 'Taskbar and Help'
        'ole-drag-drop' = 'OLE Drag and Drop'
        'ole-custom-format' = 'OLE Custom Format'
        'ole-drop-target' = 'OLE Drop Target'
        'native-hwnd-host' = 'Native HWND Host'
        'dpi-and-appearance' = 'DPI and Appearance'
        'webview2-navigation' = 'WebView2 Navigation'
        'add-control-wrapper' = 'Add Control Wrapper'
        'public-api-change' = 'Public API Change'
        'tab-workspace-state' = 'Tab Workspace State'
    }
    if ($special.ContainsKey($Id)) { return $special[$Id] }
    $acronyms = @('api', 'com', 'dpi', 'emf', 'gdi', 'hwnd', 'ide', 'mdi',
        'ole', 'png', 'sdi', 'ui')
    return (($Id -split '-') | ForEach-Object {
        if ($_ -in $acronyms) { $_.ToUpperInvariant() }
        else { $_.Substring(0, 1).ToUpperInvariant() + $_.Substring(1) }
    }) -join ' '
}

function Doc-Name([string]$Path) {
    $stem = [System.IO.Path]::GetFileNameWithoutExtension($Path)
    return Display-Name $stem
}

$categoryIds = [ordered]@{
    'Foundation and UI' = @(
        'application-window', 'native-form', 'responsive-layout', 'commands',
        'background-ui-update', 'desktop-integration', 'custom-dialog',
        'appearance-accessibility', 'property-sheet-settings', 'task-dialog',
        'control-resources', 'native-hwnd-host'
    )
    'Documents and data' = @(
        'document-state', 'multi-document-workspace-model',
        'active-document-coordination', 'document-session-persistence',
        'native-document-tabs', 'tab-workspace-state', 'split-pane-composition',
        'stable-navigation-data', 'typed-selections', 'unicode-text-file',
        'text-search', 'text-history', 'recent-files', 'single-instance',
        'sdi-notepad-reference'
    )
    'Rendering and media' = @(
        'direct2d-drawing', 'direct3d-swap-chain', 'image-viewer', 'emf-gdiplus'
    )
    'Desktop workflows' = @(
        'tray-application', 'printing-preview', 'ole-drag-drop',
        'shell-integration', 'taskbar-help', 'sqlite-viewer', 'file-folder-compare'
    )
    'Optional workspaces' = @(
        'webview2-browser', 'pdf-viewer', 'scintilla-code-editor', 'markdown-editor', 'docking-workspace',
        'windows-ribbon', 'legacy-mdi'
    )
}

$categoryById = @{}
foreach ($entry in $categoryIds.GetEnumerator()) {
    foreach ($id in $entry.Value) { $categoryById[$id] = $entry.Key }
}

$capabilitiesPath = Join-Path $ProjectRoot 'docs/capabilities.json'
$capabilities = (Get-Content $capabilitiesPath -Raw | ConvertFrom-Json).capabilities
$knownIds = @($categoryById.Keys)
foreach ($capability in $capabilities) {
    if (-not $categoryById.ContainsKey($capability.id)) {
        throw "Capability '$($capability.id)' has no Pages category."
    }
}
foreach ($id in $knownIds) {
    if ($id -notin $capabilities.id) { throw "Pages category contains unknown capability '$id'." }
}

$cards = [System.Text.StringBuilder]::new()
foreach ($category in $categoryIds.Keys) {
    [void]$cards.AppendLine("        <section class=`"capability-group`" data-capability-group=`"$(Encode-Html $category)`">")
    [void]$cards.AppendLine("          <h2>$(Encode-Html $category)</h2>")
    [void]$cards.AppendLine('          <div class="capability-grid">')
    foreach ($id in $categoryIds[$category]) {
        $capability = $capabilities | Where-Object id -eq $id
        $intent = ($capability.intent | ForEach-Object { $_.TrimEnd('.') }) -join '; '
        $symbols = ($capability.symbols | ForEach-Object { "<code>$(Encode-Html $_)</code>" }) -join ' '
        $headers = ($capability.headers | ForEach-Object { "<code>&lt;$(Encode-Html $_)&gt;</code>" }) -join ' '
        $searchTerms = @($id, $category) + @($capability.intent) +
            @($capability.symbols) + @($capability.headers)
        $search = $searchTerms -join ' '
        [void]$cards.AppendLine("            <article class=`"capability-card`" data-capability-id=`"$(Encode-Html $id)`" data-category=`"$(Encode-Html $category)`" data-search=`"$(Encode-Html $search.ToLowerInvariant())`">")
        [void]$cards.AppendLine("              <span class=`"capability-kind`">$(Encode-Html $category)</span><h3>$(Encode-Html (Display-Name $id))</h3>")
        [void]$cards.AppendLine("              <p>$(Encode-Html $intent).</p><div class=`"capability-symbols`">$symbols</div>")
        [void]$cards.AppendLine("              <div class=`"capability-headers`">$headers</div><nav aria-label=`"$(Encode-Html (Display-Name $id)) resources`">")
        if ($capability.example) {
            [void]$cards.AppendLine("                <a href=`"https://github.com/mwfl/mwfl/blob/main/$($capability.example)`">Example</a>")
        }
        if ($capability.recipe) {
            [void]$cards.AppendLine("                <a href=`"https://github.com/mwfl/mwfl/blob/main/$($capability.recipe)`">Guide</a>")
        }
        [void]$cards.AppendLine("                <a href=`"https://github.com/mwfl/mwfl/blob/main/docs/api-index.json`">API index</a>")
        [void]$cards.AppendLine('              </nav>')
        if ($capability.constraints -or $capability.tests) {
            [void]$cards.AppendLine('              <details><summary>Contracts and evidence</summary>')
            if ($capability.constraints) {
                [void]$cards.AppendLine("                <p><strong>Rules:</strong> $(Encode-Html (($capability.constraints | Select-Object -First 3) -join '; ')).</p>")
            }
            if ($capability.tests) {
                [void]$cards.AppendLine("                <p><strong>Tests:</strong> <code>$(Encode-Html (($capability.tests | Select-Object -First 4) -join '</code>, <code>'))</code></p>")
            }
            [void]$cards.AppendLine('              </details>')
        }
        [void]$cards.AppendLine('            </article>')
    }
    [void]$cards.AppendLine('          </div>')
    [void]$cards.AppendLine('        </section>')
}

$usage = [System.Text.StringBuilder]::new()
$docGroups = [ordered]@{
    'Recipes' = Get-ChildItem (Join-Path $ProjectRoot 'docs/recipes/*.md') | Where-Object Name -ne 'index.md' | Sort-Object Name
    'Tutorials' = Get-ChildItem (Join-Path $ProjectRoot 'docs/tutorials/*.md') | Sort-Object Name
}
foreach ($entry in $docGroups.GetEnumerator()) {
    [void]$usage.AppendLine("          <section><h3>$($entry.Key)</h3><ul class=`"usage-list`">")
    foreach ($file in $entry.Value) {
        $relative = $file.FullName.Substring($ProjectRoot.Length + 1).Replace('\', '/')
        [void]$usage.AppendLine("            <li data-usage-path=`"$relative`"><a href=`"https://github.com/mwfl/mwfl/blob/main/$relative`">$(Encode-Html (Doc-Name $file.Name))</a></li>")
    }
    [void]$usage.AppendLine('          </ul></section>')
}

$html = @"
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="Search every mwfl capability, public symbol, example, recipe, tutorial, contract, and test from one component catalog.">
  <link rel="canonical" href="https://mwfl.github.io/components/catalog.html">
  <title>Complete component catalog &mdash; mwfl</title>
  <meta name="theme-color" content="#f7f9ff">
  <link rel="icon" type="image/svg+xml" href="../assets/mwfl-mark.svg">
  <link rel="stylesheet" href="../styles.css?v=20260810a">
  <script>try{const t=localStorage.getItem("mwfl-theme");if(t)document.documentElement.dataset.theme=t}catch{}</script>
  <script defer src="../assets/site.js?v=20260810a"></script>
</head>
<body>
  <a class="skip-link" href="#main">Skip to content</a>
  <header class="site-header docs-topbar"><div class="nav">
    <a class="brand" href="../index.html" aria-label="MWFL &mdash; Modern Windows Foundation Layer"><img src="../assets/mwfl-mark.svg" alt=""><span>MWFL</span></a>
    <nav class="nav-links" aria-label="Main navigation"><a class="active" href="index.html">Components</a><a href="../tutorial.html">Get started</a><a href="../changelog.html">Changelog</a><a href="https://github.com/mwfl/mwfl">GitHub</a><button class="theme-toggle" type="button" aria-label="Toggle color theme"></button></nav>
  </div></header>
  <main id="main" class="shell catalog-page">
    <header class="catalog-hero"><div class="eyebrow">Complete component catalog</div><h1>Every capability.<br>One searchable map.</h1><p>Browse all $($capabilities.Count) public capability slices. Each card connects intent to exact C++20 symbols, headers, runnable code, usage guidance, contracts, and automated evidence.</p>
      <div class="catalog-stats"><span><strong>$($capabilities.Count)</strong> capabilities</span><span><strong>44</strong> examples</span><span><strong>$($docGroups['Recipes'].Count)</strong> recipes</span><span><strong>$($docGroups['Tutorials'].Count)</strong> tutorials</span></div>
    </header>
    <section class="catalog-tools" aria-label="Filter components">
      <label for="component-search">Find by task, symbol, or header</label><input id="component-search" type="search" placeholder="Try docking, PrintJob, mwfl/ribbon.h..." autocomplete="off">
      <div class="catalog-filters" role="group" aria-label="Capability category"><button class="active" type="button" data-catalog-filter="all">All</button>
$(($categoryIds.Keys | ForEach-Object { "        <button type=`"button`" data-catalog-filter=`"$(Encode-Html $_)`">$(Encode-Html $_)</button>" }) -join "`n")
      </div><p class="catalog-result" role="status" aria-live="polite">Showing all $($capabilities.Count) capabilities.</p>
    </section>
    <div class="capability-groups">
$cards
    </div>
    <section id="cmake-components" class="catalog-reference"><div><div class="eyebrow">CMake composition</div><h2>Link only what the application uses.</h2><p>The core target stays small. Rendering, imaging, printing, OLE, Shell, Ribbon, MDI, graphics, WebView2, and Scintilla remain explicit components.</p></div><pre data-title="CMakeLists.txt"><code>find_package(mwfl CONFIG REQUIRED COMPONENTS printing ole shell)
target_link_libraries(my_app PRIVATE
  mwfl::mwfl
  mwfl::printing
  mwfl::ole
  mwfl::shell)</code></pre></section>
    <section id="usage-library" class="usage-library"><div class="eyebrow">All usage paths</div><h2>From first window to native integration.</h2><p>These lists are generated from every checked-in recipe and tutorial, so specialized workflows remain discoverable even when one capability card links a broader guide.</p><div class="usage-columns">
$usage
      </div></section>
    <section class="catalog-next"><div><div class="eyebrow">Before the first public preview</div><h2>Freeze one coherent API, then publish every example and contract from the same tested commit.</h2></div><p><a class="button primary" href="https://github.com/mwfl/mwfl/blob/main/docs/release-readiness.md">Read the release plan</a></p></section>
  </main>
  <footer class="site-footer"><div class="shell footer"><span>Modern Windows Foundation Layer &middot; MIT licensed &middot; Windows 10+ &middot; x64</span><nav aria-label="Footer"><a href="../building.html">Build</a><a href="https://github.com/mwfl/mwfl">Source</a></nav></div></footer>
</body>
</html>
"@

$output = Join-Path $ProjectRoot 'site/components/catalog.html'
[System.IO.File]::WriteAllText($output, $html, [System.Text.UTF8Encoding]::new($false))
Write-Host "Generated $output with $($capabilities.Count) capabilities."
