from pathlib import Path
from pydub import AudioSegment

# Pobierz ścieżkę do folderu, w którym znajduje się overlap.py
BASE_DIR = Path(__file__).resolve().parent

# Załaduj pliki z tego samego folderu
sound1 = AudioSegment.from_mp3(BASE_DIR / "combined_atv.mp3")
sound2 = AudioSegment.from_mp3(BASE_DIR / "Which 2 Stroke Sounds Best 500cc vs 600cc! #dirtbike #2stroke.mp3")

# Wydłuż krótszy plik ciszą, aby żaden dźwięk nie został ucięty
if len(sound2) > len(sound1):
    sound1 = sound1 + AudioSegment.silent(duration=len(sound2) - len(sound1))

# Nałóż oba dźwięki od samego początku (0 ms)
combined = sound1.overlay(sound2, position=0)

# Zapisz wynik w tym samym folderze
output_path = BASE_DIR / "combined_atv2.mp3"
combined.export(output_path, format="mp3")
print(f"Gotowe! Zapisano jako: {output_path}")