"""OyNIx Browser - Utility Helper Functions"""

import re
from urllib.parse import urlparse


def format_url(url, max_length=60):
    """Format URL for display."""
    if len(url) > max_length:
        return url[:max_length - 3] + "..."
    return url


def is_valid_url(url):
    """Check if string is a valid URL."""
    return url.startswith(('http://', 'https://', 'oyn://'))


def safe_filename(filename):
    """Create safe filename from string."""
    return re.sub(r'[^\w\-_.]', '_', filename)


def extract_domain(url):
    """Extract domain from URL."""
    try:
        return urlparse(url).netloc
    except Exception:
        return url


def truncate(text, max_length=200):
    """Truncate text with ellipsis."""
    if len(text) <= max_length:
        return text
    return text[:max_length - 3] + "..."
