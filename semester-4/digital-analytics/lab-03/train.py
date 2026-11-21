"""
train.py
--------
Скрипт обучения модели ResNet-18 для бинарной классификации
"человек / лошадь".

Запуск из командной строки:
    python train.py --train_dir путь/к/train --val_dir путь/к/val
    python train.py --train_dir путь/к/train          # тогда валидация будет
                                                       # автоматически отрезана
                                                       # от тренировочной выборки

Структура папок с данными (используется torchvision.datasets.ImageFolder):
    train/
        humans/
            img1.jpg
            img2.jpg
            ...
        animals/
            img1.jpg
            ...
    val/
        humans/...
        animals/...

Имена подпапок -> метки классов. По алфавиту: animals=0, humans=1.
"""

import argparse
import os
from pathlib import Path

import torch
import torch.nn as nn
from torch.utils.data import DataLoader, Subset
from torchvision import datasets, transforms
from tqdm import tqdm

from model import build_model


def parse_args():
    """Разбор аргументов командной строки."""
    parser = argparse.ArgumentParser(description="Обучение ResNet-18 для классификации человек/лошадь")
    parser.add_argument("--train_dir", type=str, required=True,
                        help="Путь до папки с тренировочным датасетом")
    parser.add_argument("--val_dir", type=str, default=None,
                        help="Путь до папки с валидационным датасетом. "
                             "Если не указан — будет отрезано 20%% от train")
    parser.add_argument("--epochs", type=int, default=10,
                        help="Количество эпох обучения")
    parser.add_argument("--batch_size", type=int, default=32,
                        help="Размер батча")
    parser.add_argument("--lr", type=float, default=1e-4,
                        help="Скорость обучения (learning rate)")
    parser.add_argument("--output", type=str, default="weights.pth",
                        help="Имя файла, куда сохранить веса лучшей модели")
    return parser.parse_args()


def get_transforms():
    """
    Возвращает пару трансформаций (train_tf, val_tf).

    Для тренировки добавляем аугментации (случайные повороты, отражения,
    изменения яркости) — это искусственно увеличивает разнообразие данных
    и снижает переобучение.

    Для валидации — только приведение к нужному размеру и нормализация.
    """
    # Числа взяты из ImageNet — для того чтобы веса предобученной модели
    # работали как ожидается, изображения нужно нормализовать так же.
    mean = [0.485, 0.456, 0.406]
    std = [0.229, 0.224, 0.225]

    train_tf = transforms.Compose([
        transforms.Resize((224, 224)),                       # ResNet ждёт 224x224
        transforms.RandomHorizontalFlip(),                   # случайно отражаем
        transforms.RandomRotation(10),                       # лёгкий поворот
        transforms.ColorJitter(brightness=0.2, contrast=0.2),# меняем яркость
        transforms.ToTensor(),                               # PIL -> тензор [0..1]
        transforms.Normalize(mean, std),                     # нормализация
    ])

    val_tf = transforms.Compose([
        transforms.Resize((224, 224)),
        transforms.ToTensor(),
        transforms.Normalize(mean, std),
    ])

    return train_tf, val_tf


def build_dataloaders(train_dir: str, val_dir: str | None, batch_size: int):
    """Создаёт даталоадеры для тренировки и валидации."""
    train_tf, val_tf = get_transforms()

    if batch_size <= 0:
        raise ValueError("batch_size должен быть положительным")
    full_dataset = datasets.ImageFolder(train_dir, transform=train_tf)
    if len(full_dataset.classes) != 2:
        raise ValueError("Ожидаются ровно два класса")
    class_names = full_dataset.classes
    if val_dir is None:
        # Отдельный Dataset сохраняет детерминированные преобразования val.
        val_dataset = datasets.ImageFolder(train_dir, transform=val_tf)
        train_indices, val_indices = [], []
        generator = torch.Generator().manual_seed(42)
        for label in range(len(class_names)):
            indices = [i for i, target in enumerate(full_dataset.targets) if target == label]
            if len(indices) < 2:
                raise ValueError("Для разделения нужны минимум два изображения каждого класса")
            order = torch.randperm(len(indices), generator=generator).tolist()
            indices = [indices[i] for i in order]
            n_val = max(1, int(len(indices) * 0.2))
            val_indices.extend(indices[:n_val])
            train_indices.extend(indices[n_val:])
        train_ds = Subset(full_dataset, train_indices)
        val_ds = Subset(val_dataset, val_indices)
    else:
        train_ds = full_dataset
        val_ds = datasets.ImageFolder(val_dir, transform=val_tf)
        if train_ds.class_to_idx != val_ds.class_to_idx:
            raise ValueError("Классы train и val должны совпадать")

    train_loader = DataLoader(train_ds, batch_size=batch_size,
                              shuffle=True, num_workers=0)
    val_loader = DataLoader(val_ds, batch_size=batch_size,
                            shuffle=False, num_workers=0)

    print(f"Классы: {class_names}")
    print(f"Размер train: {len(train_ds)}, размер val: {len(val_ds)}")
    return train_loader, val_loader, class_names


def run_one_epoch(model, loader, criterion, optimizer, device, train: bool):
    """
    Прогон одной эпохи. Если train=True — обновляем веса,
    если False — просто считаем метрики на валидации.
    """
    model.train() if train else model.eval()

    total_loss = 0.0
    correct = 0
    total = 0

    # tqdm нужен только для красивого прогресс-бара в терминале
    desc = "Train" if train else "Val  "
    iterator = tqdm(loader, desc=desc, leave=False)

    # torch.set_grad_enabled — включаем градиенты только при обучении
    with torch.set_grad_enabled(train):
        for images, labels in iterator:
            images = images.to(device)
            labels = labels.to(device)

            outputs = model(images)             # прогон вперёд: получаем логиты
            loss = criterion(outputs, labels)   # считаем ошибку (CrossEntropy)

            if train:
                optimizer.zero_grad()           # обнуляем градиенты прошлого шага
                loss.backward()                 # считаем градиенты
                optimizer.step()                # делаем шаг оптимизатора

            # Накапливаем статистику
            total_loss += loss.item() * images.size(0)
            preds = outputs.argmax(dim=1)       # выбираем класс с макс. logit
            correct += (preds == labels).sum().item()
            total += labels.size(0)

    if total == 0:
        raise ValueError("Пустая выборка")
    avg_loss = total_loss / total
    accuracy = correct / total
    return avg_loss, accuracy


def main():
    args = parse_args()
    if args.epochs <= 0 or args.lr <= 0:
        raise ValueError("epochs и lr должны быть положительными")
    torch.manual_seed(42)

    # Автоматически выбираем устройство: GPU если есть, иначе CPU.
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Используем устройство: {device}")

    # 1. Готовим данные
    train_loader, val_loader, class_names = build_dataloaders(
        args.train_dir, args.val_dir, args.batch_size
    )

    # 2. Создаём модель и переносим на устройство
    model = build_model(num_classes=len(class_names), pretrained=True)
    model = model.to(device)

    # 3. Функция потерь и оптимизатор.
    # CrossEntropyLoss — стандартная функция для задач классификации.
    # Adam — популярный адаптивный оптимизатор, хорошо работает "из коробки".
    # weight_decay — небольшая L2-регуляризация для борьбы с переобучением.
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=1e-4)

    # 4. Цикл обучения по эпохам.
    best_acc = -1.0
    for epoch in range(1, args.epochs + 1):
        print(f"\n=== Эпоха {epoch}/{args.epochs} ===")

        train_loss, train_acc = run_one_epoch(model, train_loader, criterion,
                                              optimizer, device, train=True)
        val_loss, val_acc = run_one_epoch(model, val_loader, criterion,
                                          optimizer, device, train=False)

        print(f"train loss={train_loss:.4f}  acc={train_acc*100:.2f}%  | "
              f"val loss={val_loss:.4f}  acc={val_acc*100:.2f}%")

        # 5. Сохраняем веса только если на валидации стало лучше.
        if val_acc > best_acc:
            best_acc = val_acc
            checkpoint = {
                "model_state_dict": model.state_dict(),
                "class_names": class_names,
                "best_acc": best_acc,
            }
            torch.save(checkpoint, args.output)
            print(f"  ↳ сохранены новые лучшие веса в '{args.output}' "
                  f"(val acc = {best_acc*100:.2f}%)")

    print(f"\nГотово. Лучшая точность на валидации: {best_acc*100:.2f}%")
    print(f"Веса лежат в файле: {os.path.abspath(args.output)}")


if __name__ == "__main__":
    main()
