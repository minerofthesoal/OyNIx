# Community Database Files

Drop your exported OyNIx database JSON files here to share with other users.

When an OyNIx browser connects to the internet, it will automatically scan this
folder for new files and offer to merge them into the local database (skipping
duplicates).

## File format

Export your database from OyNIx via **File > Export > Database** — this produces
a JSON file that can be placed directly into this folder.

Accepted formats:
- **OyNIx export** — `{"sites": [{"url": "...", "title": "..."}], ...}`
- **Flat list** — `[{"url": "...", "title": "..."}, ...]`
