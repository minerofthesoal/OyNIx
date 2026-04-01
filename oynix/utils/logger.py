"""Oynix Browser - Logging System"""

import logging
import os

def setup_logger():
    """Setup logging for Oynix."""
    log_dir = os.path.expanduser("~/.config/oynix")
    os.makedirs(log_dir, exist_ok=True)
    
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(os.path.join(log_dir, 'oynix.log')),
            logging.StreamHandler()
        ]
    )
    
    return logging.getLogger('oynix')

logger = setup_logger()
