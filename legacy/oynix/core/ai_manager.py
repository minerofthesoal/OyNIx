"""
Oynix AI Manager - Nano-mini-6.99-v1 Integration
Replaces generic AI with custom-configured Nano-mini model

Features:
- Auto-download ray0rf1re/Nano-mini-6.99-v1
- Custom config and tokenizer support
- Optimized for browser AI tasks
- Chat, summarization, code generation
"""

import os
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer, AutoConfig
from PyQt6.QtCore import QThread, pyqtSignal

class NanoMiniLoader(QThread):
    """Background thread to load Nano-mini model."""
    model_loaded = pyqtSignal(object, object)  # model, tokenizer
    progress_update = pyqtSignal(str)
    error_occurred = pyqtSignal(str)
    
    def __init__(self):
        super().__init__()
        self.model_id = "ray0rf1re/Nano-mini-6.99-v1"
        
    def run(self):
        """Load model in background."""
        try:
            self.progress_update.emit("📥 Downloading Nano-mini-6.99-v1...")
            
            # Load custom config
            config = AutoConfig.from_pretrained(
                self.model_id,
                trust_remote_code=True
            )
            self.progress_update.emit("✓ Custom config loaded")
            
            # Load custom tokenizer
            tokenizer = AutoTokenizer.from_pretrained(
                self.model_id,
                trust_remote_code=True
            )
            self.progress_update.emit("✓ Custom tokenizer loaded")
            
            # Load model
            self.progress_update.emit("🤖 Loading model weights...")
            model = AutoModelForCausalLM.from_pretrained(
                self.model_id,
                config=config,
                device_map="auto",
                torch_dtype=torch.float16 if torch.cuda.is_available() else torch.float32,
                low_cpu_mem_usage=True,
                trust_remote_code=True
            )
            
            self.progress_update.emit("✓ Nano-mini-6.99-v1 ready!")
            self.model_loaded.emit(model, tokenizer)
            
        except Exception as e:
            self.error_occurred.emit(f"Failed to load Nano-mini: {str(e)}")


class OynixAIManager:
    """
    Manages Nano-mini-6.99-v1 AI model for browser tasks.
    """
    
    def __init__(self):
        self.model = None
        self.tokenizer = None
        self.model_loaded = False
        self.loader_thread = None
        
    def load_model_async(self, callback=None):
        """
        Load Nano-mini model in background.
        
        Args:
            callback: Function to call when model is loaded
        """
        if self.model_loaded:
            print("✓ Model already loaded")
            if callback:
                callback(self.model, self.tokenizer)
            return
        
        print("Starting Nano-mini-6.99-v1 loader...")
        self.loader_thread = NanoMiniLoader()
        
        if callback:
            self.loader_thread.model_loaded.connect(
                lambda m, t: self.on_model_loaded(m, t, callback)
            )
        else:
            self.loader_thread.model_loaded.connect(self.on_model_loaded)
        
        self.loader_thread.progress_update.connect(print)
        self.loader_thread.error_occurred.connect(self.on_error)
        self.loader_thread.start()
    
    def on_model_loaded(self, model, tokenizer, callback=None):
        """Called when model finishes loading."""
        self.model = model
        self.tokenizer = tokenizer
        self.model_loaded = True
        print("✓ Nano-mini-6.99-v1 is ready!")
        
        if callback:
            callback(model, tokenizer)
    
    def on_error(self, error_msg):
        """Handle loading errors."""
        print(f"❌ Error: {error_msg}")
        print("Tip: Check internet connection and HuggingFace access")
    
    def generate_text(self, prompt, max_length=200, temperature=0.7):
        """
        Generate text using Nano-mini.
        
        Args:
            prompt: Input text
            max_length: Maximum tokens to generate
            temperature: Sampling temperature
        
        Returns:
            Generated text string
        """
        if not self.model_loaded:
            return "Error: Model not loaded. Please wait for initialization."
        
        try:
            inputs = self.tokenizer(prompt, return_tensors="pt").to(self.model.device)
            
            with torch.no_grad():
                outputs = self.model.generate(
                    **inputs,
                    max_new_tokens=max_length,
                    temperature=temperature,
                    do_sample=True,
                    top_p=0.95,
                    top_k=50
                )
            
            generated = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
            
            # Remove the input prompt from output
            if generated.startswith(prompt):
                generated = generated[len(prompt):].strip()
            
            return generated
            
        except Exception as e:
            return f"Generation error: {str(e)}"
    
    def chat(self, message, conversation_history=None):
        """
        Chat with Nano-mini AI.
        
        Args:
            message: User message
            conversation_history: List of previous messages
        
        Returns:
            AI response
        """
        if conversation_history is None:
            conversation_history = []
        
        # Build conversation prompt
        prompt = ""
        for role, text in conversation_history:
            prompt += f"{role}: {text}\n"
        prompt += f"User: {message}\nAssistant:"
        
        response = self.generate_text(prompt, max_length=300)
        
        # Update conversation history
        conversation_history.append(("User", message))
        conversation_history.append(("Assistant", response))
        
        return response, conversation_history
    
    def summarize_page(self, page_text, max_length=150):
        """
        Summarize a webpage using Nano-mini.
        
        Args:
            page_text: Text content from webpage
            max_length: Maximum summary length
        
        Returns:
            Summary text
        """
        # Truncate if too long
        if len(page_text) > 2000:
            page_text = page_text[:2000] + "..."
        
        prompt = f"Summarize the following text concisely:\n\n{page_text}\n\nSummary:"
        summary = self.generate_text(prompt, max_length=max_length)
        
        return summary
    
    def generate_code(self, description):
        """
        Generate code based on description.
        
        Args:
            description: What code should do
        
        Returns:
            Generated code
        """
        prompt = f"Generate Python code for: {description}\n\nCode:\n```python\n"
        code = self.generate_text(prompt, max_length=500, temperature=0.3)
        
        return code
    
    def is_ready(self):
        """Check if model is loaded and ready."""
        return self.model_loaded


# Create singleton instance
ai_manager = OynixAIManager()
