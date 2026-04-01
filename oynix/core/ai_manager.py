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
MODEL_REPO = "TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF"
MODEL_FILE = "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"
MODEL_PATH = os.path.join(MODEL_DIR, MODEL_FILE)

# Smaller fallback model
FALLBACK_REPO = "TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF"
FALLBACK_FILE = "tinyllama-1.1b-chat-v1.0.Q2_K.gguf"
FALLBACK_PATH = os.path.join(MODEL_DIR, FALLBACK_FILE)

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
                "Run: pip install huggingface-hub"
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
    """Loads the LLM into memory."""
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
                "Run: pip install llama-cpp-python"
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
        self.fallback = FallbackAssistant()
        self.conversation_history = []
        self._download_thread = None
        self._load_thread = None

    def load_model_async(self):
        """Start the model download + load pipeline."""
        if self.model_loaded:
            self.status_update.emit("AI model already loaded.")
            self.model_ready.emit()
            return

        # Check if model is already downloaded
        for path in [MODEL_PATH, FALLBACK_PATH]:
            if os.path.isfile(path):
                self.model_path = path
                self._start_loading(path)
                return

        # Need to download
        if not HAS_HF:
            self.status_update.emit(
                "AI running in fallback mode (install huggingface-hub for full AI)."
            )
            return

        self.status_update.emit("Starting AI model download...")
        self._download_thread = ModelDownloader(
            MODEL_REPO, MODEL_FILE, MODEL_DIR
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
        """Model is loaded and ready."""
        self.model = model
        self.model_loaded = True
        self.status_update.emit("Nyx AI is ready!")
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
        if not self.model_loaded or self.model is None:
            return self.fallback.generate(message)

        try:
            # Build conversation for TinyLlama chat format
            self.conversation_history.append({
                "role": "user",
                "content": message
            })

            # Keep last 10 messages for context
            recent = self.conversation_history[-10:]

            # TinyLlama chat template
            prompt = "<|system|>\nYou are Nyx, a helpful AI assistant built into the OyNIx web browser. You help users search the web, summarize pages, and answer questions. Be concise and helpful.</s>\n"
            for msg in recent:
                if msg["role"] == "user":
                    prompt += f"<|user|>\n{msg['content']}</s>\n"
                else:
                    prompt += f"<|assistant|>\n{msg['content']}</s>\n"
            prompt += "<|assistant|>\n"

            response = self.model(
                prompt,
                max_tokens=512,
                temperature=0.7,
                top_p=0.9,
                stop=["</s>", "<|user|>", "<|system|>"],
                echo=False,
            )

            text = response["choices"][0]["text"].strip()

            self.conversation_history.append({
                "role": "assistant",
                "content": text
            })

            return text

        except Exception as e:
            return f"Error generating response: {e}"

    def summarize(self, page_text, max_length=300):
        """Summarize text content from a web page."""
        if not self.model_loaded or self.model is None:
            return self.fallback.generate("summarize " + page_text[:100])

        try:
            # Truncate to reasonable length
            text = page_text[:3000]
            prompt = (
                "<|system|>\nYou are Nyx, a helpful AI assistant. "
                "Summarize the following text concisely.</s>\n"
                f"<|user|>\nSummarize this:\n\n{text}</s>\n"
                "<|assistant|>\n"
            )

            response = self.model(
                prompt,
                max_tokens=max_length,
                temperature=0.3,
                stop=["</s>"],
                echo=False,
            )

            return response["choices"][0]["text"].strip()

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
        if self.model_loaded:
            return {
                'status': 'ready',
                'model': os.path.basename(self.model_path or 'unknown'),
                'backend': 'llama.cpp',
            }
        elif self._download_thread and self._download_thread.isRunning():
            return {'status': 'downloading', 'model': MODEL_FILE}
        elif self._load_thread and self._load_thread.isRunning():
            return {'status': 'loading', 'model': MODEL_FILE}
        else:
            return {'status': 'fallback', 'model': 'rule-based'}


# Singleton
ai_manager = OynixAIManager()
