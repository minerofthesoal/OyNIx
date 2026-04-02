"""Oynix Browser - Utility Helper Functions"""

def format_url(url):
    """Format URL for display."""
    if len(url) > 50:
        return url[:47] + "..."
    return url

def is_valid_url(url):
    """Check if string is valid URL."""
    return url.startswith(('http://', 'https://', 'oyn://'))

def safe_filename(filename):
    """Create safe filename from string."""
    import re
    return re.sub(r'[^\w\-_.]', '_', filename)
