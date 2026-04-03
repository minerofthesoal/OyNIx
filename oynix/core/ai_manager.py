"""
OyNIx Browser - Local LLM Manager
Auto-downloads and runs a local LLM with zero external setup required.

Strategy:
1. On first launch, auto-downloads a small GGUF model via huggingface_hub
2. Uses llama-cpp-python for CPU inference (no GPU needed)
3. Falls back to a simple rule-based assistant if deps are missing
4. Provides chat, summarization, and search-assist capabilities
"""

import os
import sys
import json
import threading
from pathlib import Path

from PyQt6.QtCore import QThread, pyqtSignal, QObject

# ── Model Configuration ──────────────────────────────────────────────
MODEL_DIR = os.path.join(os.path.expanduser("~"), ".config", "oynix", "models")
MODEL_CONFIG_FILE = os.path.join(MODEL_DIR, "model_config.json")

# Available models — users can switch in settings
AVAILABLE_MODELS = {
    'tinyllama': {
        'name': 'TinyLlama 1.1B Chat',
        'repo': 'TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF',
        'file': 'tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf',
        'format': 'gguf',
        'description': 'Fast, lightweight 1.1B model for basic chat',
        'chat_template': 'tinyllama',
    },
    'hyper-nix': {
        'name': 'HyperNix.1 (Custom GPT-2)',
        'repo': 'ray0rf1re/hyper-nix.1',
        'file': None,  # Transformer model, no single GGUF file
        'format': 'transformers',
        'description': 'Custom GPT-2 tokenizer model, optimized for OyNIx',
        'chat_template': 'chatml',
    },
    'nix-mm': {
        'name': 'Nix 2.6 Multimodal (Qwen fine-tune)',
        'repo': 'Nix-ai/Nix2.6-mm',
        'file': None,  # Transformer model
        'format': 'transformers',
        'description': 'Qwen-based multimodal model fine-tuned for Nix',
        'chat_template': 'qwen',
    },
}

# Default model
DEFAULT_MODEL = 'tinyllama'
MODEL_REPO = AVAILABLE_MODELS[DEFAULT_MODEL]['repo']
MODEL_FILE = AVAILABLE_MODELS[DEFAULT_MODEL]['file']
MODEL_PATH = os.path.join(MODEL_DIR, MODEL_FILE) if MODEL_FILE else None

# Smaller fallback model
FALLBACK_REPO = "TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF"
FALLBACK_FILE = "tinyllama-1.1b-chat-v1.0.Q2_K.gguf"
FALLBACK_PATH = os.path.join(MODEL_DIR, FALLBACK_FILE)

# Try transformers for HyperNix/Nix models
try:
    from transformers import AutoModelForCausalLM, AutoTokenizer, pipeline
    HAS_TRANSFORMERS = True
except ImportError:
    HAS_TRANSFORMERS = False

# Try imports
try:
    from llama_cpp import Llama
    HAS_LLAMA = True
except ImportError:
    HAS_LLAMA = False

try:
    from huggingface_hub import hf_hub_download
    HAS_HF = True
except ImportError:
    HAS_HF = False


class ModelDownloader(QThread):
    """Downloads the LLM model in the background."""
    progress = pyqtSignal(str)
    finished_ok = pyqtSignal(str)   # model_path
    error = pyqtSignal(str)

    def __init__(self, repo_id, filename, dest_dir):
        super().__init__()
        self.repo_id = repo_id
        self.filename = filename
        self.dest_dir = dest_dir

    def run(self):
        if not HAS_HF:
            self.error.emit(
                "huggingface_hub not installed. "
                "Run: pip install --break-system-packages huggingface-hub"
            )
            return

        dest = os.path.join(self.dest_dir, self.filename)
        if os.path.isfile(dest):
            self.progress.emit(f"Model already downloaded: {self.filename}")
            self.finished_ok.emit(dest)
            return

        try:
            os.makedirs(self.dest_dir, exist_ok=True)
            self.progress.emit(
                f"Downloading {self.filename} from {self.repo_id}...\n"
                "This is a one-time download (~700MB). Please wait."
            )

            path = hf_hub_download(
                repo_id=self.repo_id,
                filename=self.filename,
                local_dir=self.dest_dir,
                local_dir_use_symlinks=False,
            )

            self.progress.emit("Download complete!")
            self.finished_ok.emit(path)

        except Exception as e:
            # Try fallback smaller model
            self.progress.emit(f"Primary download failed: {e}")
            self.progress.emit("Trying smaller fallback model...")
            try:
                path = hf_hub_download(
                    repo_id=FALLBACK_REPO,
                    filename=FALLBACK_FILE,
                    local_dir=self.dest_dir,
                    local_dir_use_symlinks=False,
                )
                self.finished_ok.emit(path)
            except Exception as e2:
                self.error.emit(
                    f"Failed to download model: {e2}\n"
                    "AI features will use fallback mode."
                )


class ModelLoader(QThread):
    """Loads the LLM into memory (GGUF via llama.cpp)."""
    progress = pyqtSignal(str)
    loaded = pyqtSignal(object)  # Llama instance
    error = pyqtSignal(str)

    def __init__(self, model_path):
        super().__init__()
        self.model_path = model_path

    def run(self):
        if not HAS_LLAMA:
            self.error.emit(
                "llama-cpp-python not installed. "
                "Run: pip install --break-system-packages llama-cpp-python"
            )
            return

        try:
            self.progress.emit("Loading LLM into memory...")
            model = Llama(
                model_path=self.model_path,
                n_ctx=2048,
                n_threads=4,
                n_gpu_layers=0,  # CPU only for max compatibility
                verbose=False,
            )
            self.progress.emit("LLM loaded and ready!")
            self.loaded.emit(model)
        except Exception as e:
            self.error.emit(f"Failed to load model: {e}")


class TransformersModelLoader(QThread):
    """Loads a HuggingFace Transformers model (for HyperNix / Nix-MM)."""
    progress = pyqtSignal(str)
    loaded = pyqtSignal(object)  # dict with model + tokenizer
    error = pyqtSignal(str)

    def __init__(self, repo_id, model_key):
        super().__init__()
        self.repo_id = repo_id
        self.model_key = model_key

    def run(self):
        if not HAS_TRANSFORMERS:
            self.error.emit(
                "transformers not installed. "
                "Run: pip install transformers torch"
            )
            return

        try:
            self.progress.emit(f"Loading {self.repo_id} (this may download files)...")
            cache_dir = os.path.join(MODEL_DIR, self.model_key)

            tokenizer = AutoTokenizer.from_pretrained(
                self.repo_id, cache_dir=cache_dir, trust_remote_code=True)

            model = AutoModelForCausalLM.from_pretrained(
                self.repo_id, cache_dir=cache_dir, trust_remote_code=True,
                low_cpu_mem_usage=True)

            self.progress.emit(f"{self.repo_id} loaded!")
            self.loaded.emit({
                'type': 'transformers',
                'model': model,
                'tokenizer': tokenizer,
                'key': self.model_key,
            })
        except Exception as e:
            self.error.emit(f"Failed to load {self.repo_id}: {e}")


class FallbackAssistant:
    """
    Simple rule-based assistant used when LLM is not available.
    Provides basic search assistance and canned responses.
    """

    def generate(self, prompt, max_tokens=200):
        """Generate a response using simple heuristics."""
        prompt_lower = prompt.lower()

        if any(w in prompt_lower for w in ['hello', 'hi', 'hey']):
            return ("Hello! I'm Nyx, your local AI assistant. "
                    "I can help you search, summarize pages, and more. "
                    "Note: Full AI features require the LLM model to be loaded.")

        if any(w in prompt_lower for w in ['summarize', 'summary', 'tldr']):
            return ("I'd love to summarize that for you, but the full LLM model "
                    "isn't loaded yet. Once it downloads, I can provide "
                    "intelligent summaries of any page you visit.")

        if any(w in prompt_lower for w in ['search', 'find', 'look up']):
            return ("You can search using the Nyx search engine! "
                    "It combines your browsing history index, "
                    "our curated database of 1400+ sites, and DuckDuckGo "
                    "web results. Just type in the URL bar.")

        if any(w in prompt_lower for w in ['help', 'what can you do']):
            return (
                "I'm Nyx, your browser's AI assistant. I can:\n"
                "- Chat with you about anything\n"
                "- Summarize web pages\n"
                "- Help you search the web\n"
                "- Explain complex content simply\n"
                "- Answer questions about your browsing\n\n"
                "The full LLM model will auto-download in the background "
                "for more intelligent responses."
            )

        return (
            "I'm processing your request. The full LLM model is still "
            "loading in the background. Once ready, I'll be able to "
            "give you much more detailed and intelligent responses. "
            "In the meantime, try using the Nyx search engine!"
        )


class OynixAIManager(QObject):
    """
    Manages the local LLM lifecycle:
    1. Auto-download on first run
    2. Load into memory
    3. Provide inference for chat, summarize, etc.
    4. Falls back to FallbackAssistant when model is not available
    """

    status_update = pyqtSignal(str)
    model_ready = pyqtSignal()

    def __init__(self):
        super().__init__()
        self.model = None
        self.model_loaded = False
        self.model_path = None
        self.model_type = 'gguf'       # 'gguf' or 'transformers'
        self.model_key = DEFAULT_MODEL  # active model key
        self._tokenizer = None         # for transformers models
        self._tf_model = None          # for transformers models
        self.fallback = FallbackAssistant()
        self.conversation_history = []
        self._download_thread = None
        self._load_thread = None
        self._load_model_config()

    def _load_model_config(self):
        """Load user's model preference."""
        try:
            with open(MODEL_CONFIG_FILE) as f:
                cfg = json.load(f)
                self.model_key = cfg.get('active_model', DEFAULT_MODEL)
        except (FileNotFoundError, json.JSONDecodeError):
            pass

    def _save_model_config(self):
        os.makedirs(MODEL_DIR, exist_ok=True)
        with open(MODEL_CONFIG_FILE, 'w') as f:
            json.dump({'active_model': self.model_key}, f)

    def get_available_models(self):
        """Return list of available models for the settings UI."""
        return AVAILABLE_MODELS

    def switch_model(self, model_key):
        """Switch to a different AI model."""
        if model_key not in AVAILABLE_MODELS:
            return False
        self.model_key = model_key
        self.model = None
        self.model_loaded = False
        self._tf_model = None
        self._tokenizer = None
        self._save_model_config()
        self.load_model_async()
        return True

    def load_model_async(self):
        """Start the model download + load pipeline."""
        if self.model_loaded:
            self.status_update.emit("AI model already loaded.")
            self.model_ready.emit()
            return

        model_info = AVAILABLE_MODELS.get(self.model_key)
        if not model_info:
            self.model_key = DEFAULT_MODEL
            model_info = AVAILABLE_MODELS[DEFAULT_MODEL]

        # Transformers models (HyperNix, Nix-MM)
        if model_info['format'] == 'transformers':
            if not HAS_TRANSFORMERS:
                self.status_update.emit(
                    f"AI: {model_info['name']} requires 'transformers' package. "
                    "Run: pip install transformers torch. Using fallback."
                )
                return
            self.status_update.emit(f"Loading {model_info['name']}...")
            self._load_thread = TransformersModelLoader(
                model_info['repo'], self.model_key)
            self._load_thread.progress.connect(self.status_update.emit)
            self._load_thread.loaded.connect(self._on_transformers_loaded)
            self._load_thread.error.connect(self._on_load_error)
            self._load_thread.start()
            return

        # GGUF models (TinyLlama)
        model_file = model_info['file']
        model_path = os.path.join(MODEL_DIR, model_file) if model_file else None

        # Check if model is already downloaded
        if model_path and os.path.isfile(model_path):
            self.model_path = model_path
            self._start_loading(model_path)
            return
        if os.path.isfile(FALLBACK_PATH):
            self.model_path = FALLBACK_PATH
            self._start_loading(FALLBACK_PATH)
            return

        # Need to download
        if not HAS_HF:
            self.status_update.emit(
                "AI running in fallback mode (install huggingface-hub for full AI)."
            )
            return

        self.status_update.emit("Starting AI model download...")
        self._download_thread = ModelDownloader(
            model_info['repo'], model_file, MODEL_DIR
        )
        self._download_thread.progress.connect(self.status_update.emit)
        self._download_thread.finished_ok.connect(self._on_download_complete)
        self._download_thread.error.connect(self._on_download_error)
        self._download_thread.start()

    def _on_download_complete(self, path):
        """Model downloaded, now load it."""
        self.model_path = path
        self._start_loading(path)

    def _on_download_error(self, error_msg):
        """Download failed, use fallback."""
        self.status_update.emit(f"AI: {error_msg}")

    def _start_loading(self, path):
        """Load the downloaded model."""
        if not HAS_LLAMA:
            self.status_update.emit(
                "AI: llama-cpp-python not installed. Using fallback assistant."
            )
            return

        self._load_thread = ModelLoader(path)
        self._load_thread.progress.connect(self.status_update.emit)
        self._load_thread.loaded.connect(self._on_model_loaded)
        self._load_thread.error.connect(self._on_load_error)
        self._load_thread.start()

    def _on_model_loaded(self, model):
        """GGUF model is loaded and ready."""
        self.model = model
        self.model_loaded = True
        self.model_type = 'gguf'
        self.status_update.emit("Nyx AI is ready!")
        self.model_ready.emit()

    def _on_transformers_loaded(self, result):
        """Transformers model is loaded and ready."""
        self._tf_model = result['model']
        self._tokenizer = result['tokenizer']
        self.model_loaded = True
        self.model_type = 'transformers'
        self.model_key = result['key']
        model_info = AVAILABLE_MODELS.get(self.model_key, {})
        self.status_update.emit(f"AI ready: {model_info.get('name', self.model_key)}")
        self.model_ready.emit()

    def _on_load_error(self, error_msg):
        """Load failed."""
        self.status_update.emit(f"AI load error: {error_msg}")

    def is_ready(self):
        """Check if full LLM is loaded."""
        return self.model_loaded

    def chat(self, message):
        """
        Chat with the AI. Uses LLM if available, fallback otherwise.

        Returns:
            str: AI response
        """
        if not self.model_loaded:
            return self.fallback.generate(message)

        self.conversation_history.append({"role": "user", "content": message})
        recent = self.conversation_history[-10:]

        try:
            if self.model_type == 'transformers' and self._tf_model and self._tokenizer:
                text = self._chat_transformers(recent)
            elif self.model is not None:
                text = self._chat_gguf(recent)
            else:
                return self.fallback.generate(message)

            self.conversation_history.append({"role": "assistant", "content": text})
            return text
        except Exception as e:
            return f"Error generating response: {e}"

    def _chat_gguf(self, recent):
        """Chat using llama.cpp GGUF model (TinyLlama)."""
        prompt = "<|system|>\nYou are Nyx, a helpful AI assistant built into the OyNIx web browser. You help users search the web, summarize pages, and answer questions. Be concise and helpful.</s>\n"
        for msg in recent:
            if msg["role"] == "user":
                prompt += f"<|user|>\n{msg['content']}</s>\n"
            else:
                prompt += f"<|assistant|>\n{msg['content']}</s>\n"
        prompt += "<|assistant|>\n"

        response = self.model(
            prompt, max_tokens=512, temperature=0.7, top_p=0.9,
            stop=["</s>", "<|user|>", "<|system|>"], echo=False,
        )
        return response["choices"][0]["text"].strip()

    def _chat_transformers(self, recent):
        """Chat using HuggingFace Transformers model (HyperNix / Nix-MM)."""
        model_info = AVAILABLE_MODELS.get(self.model_key, {})
        template = model_info.get('chat_template', 'chatml')

        # Build prompt based on template
        if template == 'qwen':
            prompt = "<|im_start|>system\nYou are Nyx, a helpful AI assistant in the OyNIx browser.<|im_end|>\n"
            for msg in recent:
                role = msg["role"]
                prompt += f"<|im_start|>{role}\n{msg['content']}<|im_end|>\n"
            prompt += "<|im_start|>assistant\n"
            stop_tokens = ["<|im_end|>", "<|im_start|>"]
        else:
            # ChatML format (default for HyperNix/GPT-2)
            prompt = "<|system|>You are Nyx, a helpful AI assistant in the OyNIx browser.\n"
            for msg in recent:
                role = msg["role"]
                prompt += f"<|{role}|>{msg['content']}\n"
            prompt += "<|assistant|>"
            stop_tokens = ["<|user|>", "<|system|>", "<|endoftext|>"]

        inputs = self._tokenizer(prompt, return_tensors="pt", truncation=True, max_length=1024)
        import torch
        with torch.no_grad():
            outputs = self._tf_model.generate(
                **inputs, max_new_tokens=256, temperature=0.7,
                top_p=0.9, do_sample=True, pad_token_id=self._tokenizer.eos_token_id
            )
        full_text = self._tokenizer.decode(outputs[0], skip_special_tokens=False)
        # Extract only the new generation
        response = full_text[len(prompt):]
        # Strip stop tokens
        for st in stop_tokens:
            if st in response:
                response = response[:response.index(st)]
        return response.strip()

    def summarize(self, page_text, max_length=300):
        """Summarize text content from a web page."""
        if not self.model_loaded:
            return self.fallback.generate("summarize " + page_text[:100])

        text = page_text[:3000]
        try:
            if self.model_type == 'transformers' and self._tf_model:
                return self._chat_transformers([
                    {"role": "user", "content": f"Summarize this concisely:\n\n{text}"}
                ])
            elif self.model is not None:
                prompt = (
                    "<|system|>\nYou are Nyx, a helpful AI assistant. "
                    "Summarize the following text concisely.</s>\n"
                    f"<|user|>\nSummarize this:\n\n{text}</s>\n"
                    "<|assistant|>\n"
                )
                response = self.model(
                    prompt, max_tokens=max_length, temperature=0.3,
                    stop=["</s>"], echo=False,
                )
                return response["choices"][0]["text"].strip()
            else:
                return self.fallback.generate("summarize " + text[:100])
        except Exception as e:
            return f"Summarization error: {e}"

    def search_assist(self, query, search_results):
        """
        AI-assisted search: analyze results and provide a summary answer.
        """
        if not self.model_loaded or self.model is None:
            return None  # Skip AI assist when not ready

        try:
            context = "\n".join(
                f"- {r['title']}: {r.get('description', '')[:100]}"
                for r in search_results[:5]
            )

            prompt = (
                "<|system|>\nYou are Nyx search assistant. "
                "Based on the search results, give a brief helpful answer.</s>\n"
                f"<|user|>\nQuery: {query}\n\nResults:\n{context}</s>\n"
                "<|assistant|>\n"
            )

            response = self.model(
                prompt,
                max_tokens=200,
                temperature=0.5,
                stop=["</s>"],
                echo=False,
            )

            return response["choices"][0]["text"].strip()

        except Exception:
            return None

    def clear_history(self):
        """Clear conversation history."""
        self.conversation_history.clear()

    def get_status(self):
        """Get current AI status info."""
        model_info = AVAILABLE_MODELS.get(self.model_key, {})
        model_name = model_info.get('name', self.model_key)
        if self.model_loaded:
            backend = 'transformers' if self.model_type == 'transformers' else 'llama.cpp'
            return {'status': 'ready', 'model': model_name, 'backend': backend}
        elif self._download_thread and self._download_thread.isRunning():
            return {'status': 'downloading', 'model': model_name}
        elif self._load_thread and self._load_thread.isRunning():
            return {'status': 'loading', 'model': model_name}
        else:
            return {'status': 'fallback', 'model': 'rule-based'}


# Lazy singleton - must not create QObject before QApplication exists
_ai_manager = None

def _get_ai_manager():
    global _ai_manager
    if _ai_manager is None:
        _ai_manager = OynixAIManager()
    return _ai_manager

class _AIManagerProxy:
    """Proxy that delays OynixAIManager creation until first access."""
    def __getattr__(self, name):
        return getattr(_get_ai_manager(), name)

ai_manager = _AIManagerProxy()
