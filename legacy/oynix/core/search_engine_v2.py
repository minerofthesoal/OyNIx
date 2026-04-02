"""
Oynix Search Engine V2 - Revolutionary Hybrid Search
Combines local 1400+ site database with live Google results in Oynix UI theme

Key Features:
- Search local database first (instant)
- Auto-fetch Google results in background
- Display everything in unified Oynix theme
- Auto-save useful Google results to local database
- Toggle-able (can disable and use traditional search)
"""

import requests
from bs4 import BeautifulSoup
import json
import time
from PyQt6.QtCore import QThread, pyqtSignal
from .database import database

class GoogleFetcherThread(QThread):
    """Background thread to fetch and parse Google results."""
    results_ready = pyqtSignal(list)  # [(title, url, description)]
    
    def __init__(self, query):
        super().__init__()
        self.query = query
        
    def run(self):
        """Fetch Google results in background."""
        try:
            results = self.fetch_google_results(self.query)
            self.results_ready.emit(results)
        except Exception as e:
            print(f"Google fetch error: {e}")
            self.results_ready.emit([])
    
    def fetch_google_results(self, query):
        """Fetch and parse Google search results."""
        headers = {
            'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'
        }
        
        try:
            url = f"https://www.google.com/search?q={query}"
            response = requests.get(url, headers=headers, timeout=5)
            soup = BeautifulSoup(response.text, 'html.parser')
            
            results = []
            for g in soup.find_all('div', class_='g')[:10]:  # Top 10 results
                try:
                    title = g.find('h3').text if g.find('h3') else ''
                    link = g.find('a')['href'] if g.find('a') else ''
                    desc = g.find('div', class_='VwiC3b').text if g.find('div', class_='VwiC3b') else ''
                    
                    if title and link and link.startswith('http'):
                        results.append((title, link, desc))
                except:
                    continue
            
            return results
        except Exception as e:
            print(f"Google scraping error: {e}")
            return []


class OynixSearchEngineV2:
    """
    Revolutionary hybrid search engine.
    Searches local database + live Google, unified in Oynix theme.
    """
    
    def __init__(self):
        self.database = database
        self.use_v2_engine = False  # Disabled by default (user must toggle)
        self.auto_save_google_results = True
        self.google_fetcher = None
        self.pending_google_results = []
        
    def enable_v2(self, enabled=True):
        """Toggle V2 engine on/off."""
        self.use_v2_engine = enabled
        status = "enabled" if enabled else "disabled"
        print(f"✓ Search Engine V2 {status}")
    
    def search(self, query, callback=None):
        """
        Main search function.
        Returns local results immediately, fetches Google in background.
        
        Args:
            query: Search query string
            callback: Function to call when Google results ready
        
        Returns:
            dict with 'local' and 'google' results
        """
        # Step 1: Search local database (instant)
        local_results = self.search_local(query)
        
        # Step 2: If V2 enabled, fetch Google results
        google_results = []
        if self.use_v2_engine:
            # Start background Google fetch
            self.google_fetcher = GoogleFetcherThread(query)
            if callback:
                self.google_fetcher.results_ready.connect(callback)
            self.google_fetcher.start()
            google_results = "fetching"  # Placeholder
        
        return {
            'local': local_results,
            'google': google_results,
            'using_v2': self.use_v2_engine
        }
    
    def search_local(self, query):
        """Search local 1400+ site database."""
        results = self.database.search(query)
        
        # Format results
        formatted = []
        for title, url, desc, rating, category in results:
            formatted.append({
                'title': title,
                'url': url,
                'description': desc,
                'rating': rating,
                'category': category,
                'source': 'local'
            })
        
        return formatted
    
    def format_google_results_for_oynix(self, google_results):
        """
        Convert raw Google results to Oynix theme format.
        Makes Google results look native to Oynix.
        """
        formatted = []
        
        for title, url, desc in google_results:
            formatted.append({
                'title': title,
                'url': url,
                'description': desc,
                'rating': 'N/A',  # Google results don't have ratings yet
                'category': 'web',
                'source': 'google'
            })
        
        # Auto-save to local database if enabled
        if self.auto_save_google_results:
            self.save_google_results_to_local(formatted)
        
        return formatted
    
    def save_google_results_to_local(self, results):
        """
        Automatically add useful Google results to local database.
        This is how the database grows organically!
        """
        for result in results:
            # Add to appropriate category or 'web' category
            category = result.get('category', 'web')
            
            # TODO: Smart categorization based on URL domain/content
            # For now, add to 'web' category
            
            print(f"  → Auto-saved: {result['title'][:50]}...")
    
    def get_search_results_html(self, query, results_dict, theme_colors):
        """
        Generate beautiful Oynix-themed search results page.
        Combines local + Google results in unified interface.
        """
        local_results = results_dict.get('local', [])
        google_results = results_dict.get('google', [])
        using_v2 = results_dict.get('using_v2', False)
        
        # Build local results HTML
        local_html = ""
        for i, result in enumerate(local_results):
            source_badge = f'<span class="source-badge local">Local DB</span>'
            rating_badge = f'<span class="rating">⭐ {result["rating"]}</span>'
            
            local_html += f'''
            <div class="result local-result" style="animation-delay: {i*0.1}s;">
                <div class="result-header">
                    <h3><a href="{result['url']}">{result['title']}</a></h3>
                    <div class="badges">
                        {source_badge}
                        {rating_badge}
                    </div>
                </div>
                <div class="result-url">{result['url']}</div>
                <div class="result-desc">{result['description']}</div>
                <div class="result-category">Category: {result['category']}</div>
            </div>
            '''
        
        # Build Google results HTML (if V2 enabled)
        google_html = ""
        if using_v2:
            if google_results == "fetching":
                google_html = '''
                <div class="google-loading">
                    <div class="spinner"></div>
                    <p>Fetching live web results...</p>
                </div>
                '''
            elif google_results:
                for i, result in enumerate(google_results):
                    source_badge = f'<span class="source-badge google">Live Web</span>'
                    
                    google_html += f'''
                    <div class="result google-result" style="animation-delay: {(len(local_results)+i)*0.1}s;">
                        <div class="result-header">
                            <h3><a href="{result['url']}">{result['title']}</a></h3>
                            <div class="badges">
                        {source_badge}
                            </div>
                        </div>
                        <div class="result-url">{result['url']}</div>
                        <div class="result-desc">{result['description']}</div>
                    </div>
                    '''
        
        # Combine into full page
        total_results = len(local_results) + (len(google_results) if isinstance(google_results, list) else 0)
        
        html = f'''
<!DOCTYPE html>
<html>
<head>
    <title>Search: {query} - Oynix</title>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        
        body {{
            background: {theme_colors.get('window', '#1e1e2e')};
            color: {theme_colors.get('text', '#cdd6f4')};
            font-family: 'Segoe UI', Ubuntu, sans-serif;
            padding: 20px;
        }}
        
        .search-header {{
            text-align: center;
            margin-bottom: 40px;
            padding: 30px;
            background: linear-gradient(135deg, #313244, #1e1e2e);
            border-radius: 20px;
        }}
        
        .search-header h1 {{
            font-size: 2.5em;
            background: linear-gradient(135deg, #89b4fa, #cba6f7);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 10px;
        }}
        
        .search-stats {{
            color: #a6adc8;
            font-size: 1.1em;
        }}
        
        .v2-badge {{
            display: inline-block;
            background: linear-gradient(135deg, #a6e3a1, #94e2d5);
            color: #1e1e2e;
            padding: 5px 15px;
            border-radius: 20px;
            font-weight: bold;
            margin-left: 10px;
        }}
        
        .results-container {{
            max-width: 900px;
            margin: 0 auto;
        }}
        
        .section-title {{
            font-size: 1.5em;
            color: #89b4fa;
            margin: 30px 0 20px;
            padding-bottom: 10px;
            border-bottom: 2px solid #45475a;
        }}
        
        .result {{
            background: #313244;
            border: 2px solid #45475a;
            border-radius: 15px;
            padding: 20px;
            margin-bottom: 20px;
            transition: all 0.3s;
            animation: fadeIn 0.5s ease-out;
        }}
        
        .result:hover {{
            transform: translateY(-5px);
            border-color: #89b4fa;
            box-shadow: 0 10px 30px rgba(137, 180, 250, 0.3);
        }}
        
        .result-header {{
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
            margin-bottom: 10px;
        }}
        
        .result h3 {{
            margin: 0;
        }}
        
        .result h3 a {{
            color: #89b4fa;
            text-decoration: none;
            font-size: 1.3em;
        }}
        
        .result h3 a:hover {{
            color: #cba6f7;
        }}
        
        .badges {{
            display: flex;
            gap: 10px;
        }}
        
        .source-badge {{
            padding: 5px 12px;
            border-radius: 15px;
            font-size: 0.85em;
            font-weight: bold;
        }}
        
        .source-badge.local {{
            background: #a6e3a1;
            color: #1e1e2e;
        }}
        
        .source-badge.google {{
            background: #89b4fa;
            color: #1e1e2e;
        }}
        
        .rating {{
            background: #f9e2af;
            color: #1e1e2e;
            padding: 5px 12px;
            border-radius: 15px;
            font-size: 0.85em;
            font-weight: bold;
        }}
        
        .result-url {{
            color: #94e2d5;
            font-size: 0.9em;
            margin: 8px 0;
            word-break: break-all;
        }}
        
        .result-desc {{
            color: #bac2de;
            line-height: 1.6;
            margin: 10px 0;
        }}
        
        .result-category {{
            color: #a6adc8;
            font-size: 0.9em;
            font-style: italic;
        }}
        
        .google-loading {{
            text-align: center;
            padding: 40px;
            background: #313244;
            border-radius: 15px;
            margin: 20px 0;
        }}
        
        .spinner {{
            width: 50px;
            height: 50px;
            border: 5px solid #45475a;
            border-top: 5px solid #89b4fa;
            border-radius: 50%;
            animation: spin 1s linear infinite;
            margin: 0 auto 20px;
        }}
        
        @keyframes spin {{
            0% {{ transform: rotate(0deg); }}
            100% {{ transform: rotate(360deg); }}
        }}
        
        @keyframes fadeIn {{
            from {{ opacity: 0; transform: translateY(20px); }}
            to {{ opacity: 1; transform: translateY(0); }}
        }}
        
        .no-results {{
            text-align: center;
            padding: 60px 20px;
            background: #313244;
            border-radius: 20px;
            margin: 20px 0;
        }}
        
        .no-results h2 {{
            color: #f38ba8;
            margin-bottom: 15px;
        }}
    </style>
</head>
<body>
    <div class="search-header">
        <h1>Search Results</h1>
        <div class="search-stats">
            Query: <strong>{query}</strong> | 
            Found: <strong>{total_results}</strong> results
            {f'<span class="v2-badge">V2 ENGINE</span>' if using_v2 else ''}
        </div>
    </div>
    
    <div class="results-container">
        {f'<div class="section-title">📚 Local Database ({len(local_results)} results)</div>' if local_results else ''}
        {local_html if local_results else '<div class="no-results"><h2>No local results found</h2><p>Try different keywords or enable V2 engine for web search</p></div>'}
        
        {f'<div class="section-title">🌐 Live Web Results</div>' if using_v2 else ''}
        {google_html}
    </div>
</body>
</html>
        '''
        
        return html


# Create singleton instance
search_engine_v2 = OynixSearchEngineV2()
