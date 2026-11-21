"""
download_dataset.py
-------------------
Удобный скрипт для скачивания готового датасета "Horses or Humans"
с Kaggle через библиотеку kagglehub.

Этот датасет уже разбит на train и validation, и подходит для нашей
бинарной классификации (horses = животные, humans = люди).
Минимальная подготовка — просто скачиваем и складываем в нужные папки.

Требует:
    pip install kagglehub

И один раз — авторизации Kaggle:
  1) Зайти на https://www.kaggle.com/settings -> Create New API Token
  2) Положить kaggle.json в ~/.kaggle/kaggle.json
"""

import shutil
from pathlib import Path

import kagglehub


def main():
    print("Скачиваю датасет 'Horses or Humans' с Kaggle...")
    # kagglehub возвращает путь до распакованного датасета в кэше.
    dataset_path = Path(kagglehub.dataset_download(
        "sanikamal/horses-or-humans-dataset"
    ))
    print(f"Скачано в: {dataset_path}")

    # Структура внутри обычно такая:
    #   horse-or-human/train/horses, horse-or-human/train/humans
    #   horse-or-human/validation/horses, horse-or-human/validation/humans
    # Найдём её автоматически — ищем папки train и validation любой вложенности.
    train_src = next((p for p in dataset_path.rglob("train") if p.is_dir()), None)
    val_src = next((p for p in dataset_path.rglob("validation") if p.is_dir()), None)

    if train_src is None or val_src is None:
        raise FileNotFoundError("В датасете не найдены train и validation")

    # Копируем в текущую папку под именами train/ и val/
    target_root = Path("data")
    target_root.mkdir(exist_ok=True)
    train_dst = target_root / "train"
    val_dst = target_root / "val"

    if train_dst.exists() or val_dst.exists():
        raise FileExistsError("data/train или data/val уже существуют; выберите другую папку")

    shutil.copytree(train_src, train_dst)
    shutil.copytree(val_src, val_dst)

    print(f"\nГотово!")
    print(f"  train: {train_dst.resolve()}")
    print(f"  val  : {val_dst.resolve()}")
    print("\nТеперь можно запускать обучение:")
    print(f"  python train.py --train_dir {train_dst} --val_dir {val_dst}")


if __name__ == "__main__":
    main()
