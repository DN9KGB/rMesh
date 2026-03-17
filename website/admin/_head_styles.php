<style>
    * { margin:0; padding:0; box-sizing:border-box; }
    body { font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif; background:#1a1a2e; color:#e0e0e0; min-height:100vh; }
    a { color:inherit; text-decoration:none; }

    /* ── Nav ── */
    nav { background:#0f172a; padding:0 24px; box-shadow:0 2px 8px rgba(0,0,0,.4); position:sticky; top:0; z-index:100; }
    .nav-inner { display:flex; align-items:center; height:60px; gap:0; }
    .nav-logo { display:flex; align-items:center; gap:10px; margin-right:32px; font-size:16px; font-weight:700; letter-spacing:2px; color:#4ecca3; }
    .nav-logo img { height:38px; }
    .nav-links { display:flex; gap:4px; flex:1; }
    .nav-link { padding:7px 14px; border-radius:6px; font-size:0.875rem; font-weight:500; color:#aaa; transition:background .15s,color .15s; }
    .nav-link:hover { background:rgba(255,255,255,.08); color:#e0e0e0; }
    .nav-link.active { background:#0f3460; color:#4ecca3; }
    .nav-logout { margin-left:auto; padding:7px 14px; border-radius:6px; font-size:0.875rem; font-weight:500; color:#e94560; border:1px solid #e9456040; transition:background .15s; }
    .nav-logout:hover { background:#e9456020; }

    /* ── Page ── */
    .page { max-width:1400px; margin:0 auto; padding:28px 24px 60px; }
    .page-title { font-size:1.4rem; font-weight:700; color:#4ecca3; margin-bottom:24px; }
    .section-title { font-size:0.95rem; color:#4ecca3; text-transform:uppercase; letter-spacing:1px; margin:28px 0 12px; }

    /* ── Stat cards ── */
    .stat-grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(160px,1fr)); gap:12px; margin-bottom:8px; }
    .stat-card { background:#16213e; border:1px solid #0f3460; border-radius:8px; padding:18px 20px; cursor:pointer; transition:border-color .2s,background .2s; }
    .stat-card:hover { border-color:#4ecca3; background:#1a2a50; }
    .stat-value { font-size:2rem; font-weight:700; color:#4ecca3; }
    .stat-label { font-size:0.8rem; color:#888; margin-top:4px; }

    /* ── Table ── */
    .table-wrap { overflow-x:auto; border-radius:8px; border:1px solid #0f3460; }
    table { width:100%; border-collapse:collapse; font-size:0.85rem; }
    thead th { background:#0f172a; color:#4ecca3; padding:10px 12px; text-align:left; font-weight:600; font-size:0.78rem; text-transform:uppercase; letter-spacing:.5px; white-space:nowrap; }
    tbody tr { border-top:1px solid #0f3460; }
    tbody tr:hover { background:#0f3460; }
    tbody td { padding:8px 12px; vertical-align:top; }
    .mono { font-family:monospace; font-size:0.8rem; }
    .strong { font-weight:600; color:#e0e0e0; }
    .err-text { color:#e94560; font-size:0.78rem; }
    .muted { color:#666; font-size:0.78rem; }

    /* ── Badges ── */
    .badge { display:inline-block; padding:2px 8px; border-radius:4px; font-size:0.75rem; font-weight:600; }
    .badge-ok   { background:#4ecca320; color:#4ecca3; }
    .badge-err  { background:#e9456020; color:#e94560; }
    .badge-info { background:#38bdf820; color:#38bdf8; }
    .badge-neu  { background:#f0a50020; color:#f0a500; }

    /* ── Filter bar ── */
    .filter-bar { display:flex; gap:8px; flex-wrap:wrap; margin-bottom:14px; align-items:center; }
    .filter-bar input, .filter-bar select {
        background:#0f172a; border:1px solid #0f3460; color:#e0e0e0;
        padding:7px 12px; border-radius:6px; font-size:0.875rem; outline:none;
        transition:border-color .2s;
    }
    .filter-bar input:focus, .filter-bar select:focus { border-color:#4ecca3; }
    .filter-bar label { font-size:0.8rem; color:#888; }

    /* ── Pagination ── */
    .pagination { display:flex; gap:6px; align-items:center; margin-top:16px; flex-wrap:wrap; }
    .pag-btn { padding:6px 14px; background:#16213e; border:1px solid #0f3460; border-radius:6px; color:#aaa; font-size:0.85rem; cursor:pointer; text-decoration:none; transition:background .15s,color .15s; }
    .pag-btn:hover { background:#0f3460; color:#e0e0e0; }
    .pag-btn.active { background:#0f3460; color:#4ecca3; border-color:#4ecca3; font-weight:700; }
    .pag-info { font-size:0.8rem; color:#666; margin-left:8px; }

    /* ── Misc ── */
    .link-more { color:#4ecca3; font-size:0.85rem; text-decoration:underline; }
    .link-more:hover { color:#38bdf8; }
    .pill-lora { display:inline-block; padding:1px 7px; border-radius:12px; font-size:0.72rem; background:#4ecca320; color:#4ecca3; }
    .pill-wifi { display:inline-block; padding:1px 7px; border-radius:12px; font-size:0.72rem; background:#a78bfa20; color:#a78bfa; }
</style>
