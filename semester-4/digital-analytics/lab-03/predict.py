"""
predict.py
----------
Скрипт классификации произвольного изображения с использованием
обученной модели. Загружает веса, прогоняет картинку через сеть,
выводит предсказанный класс и вероятности.

Запуск:
    python predict.py --image путь/к/картинке.jpg
    python predict.py --image cat.jpg --weights weights.pth
"""

import argparse

import torch
import torch.nn.functional as F
from PIL import Image
from torchvision import transforms

from model import build_model


def parse_args():
    parser = argparse.ArgumentParser(description="Классификация изображения: человек или животное")
    parser.add_argument("--image", type=str, required=True,
                        help="Путь до классифицируемого изображения")
    parser.add_argument("--weights", type=str, default="weights.pth",
                        help="Путь до файла с весами модели")
    return parser.parse_args()


def get_transform():
    """Преобразования изображения — те же, что и при валидации в train.py."""
    return transforms.Compose([
        transforms.Resize((224, 224)),
        transforms.ToTensor(),
        transforms.Normalize([0.485, 0.456, 0.406],
                             [0.229, 0.224, 0.225]),
    ])


def main():
    args = parse_args()

    # Устройство: GPU если есть, иначе CPU
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    # 1. Загружаем чекпоинт (веса + имена классов).
    #    map_location нужен, чтобы можно было запускать на CPU веса,
    #    обученные на GPU, и наоборот.
    checkpoint = torch.load(args.weights, map_location=device, weights_only=True)
    class_names = checkpoint["class_names"]

    # 2. Создаём модель и подгружаем веса.
    #    pretrained=False — нам не нужны веса ImageNet, у нас свои.
    model = build_model(num_classes=len(class_names), pretrained=False)
    model.load_state_dict(checkpoint["model_state_dict"])
    model = model.to(device)
    model.eval()  # переводим в режим инференса (отключаем dropout/batchnorm-обновления)

    # 3. Загружаем картинку и применяем те же трансформации, что и при обучении.
    with Image.open(args.image) as source:
        if source.width * source.height > 16_000_000:
            raise ValueError("Слишком большое изображение")
        image = source.convert("RGB")
    transform = get_transform()
    input_tensor = transform(image).unsqueeze(0).to(device)
    # unsqueeze(0) — добавляем размерность батча: [3,224,224] -> [1,3,224,224]

    # 4. Прогон через модель. Градиенты при инференсе не нужны.
    with torch.no_grad():
        logits = model(input_tensor)             # сырые выходы сети
        probs = F.softmax(logits, dim=1)[0]      # превращаем в вероятности
        pred_idx = int(probs.argmax().item())    # индекс самого вероятного класса
        confidence = float(probs[pred_idx].item())

    # 5. Выводим результат.
    print(f"\nИзображение: {args.image}")
    print(f"Предсказание: {class_names[pred_idx]}  "
          f"(уверенность {confidence*100:.2f}%)")
    print("\nВероятности по всем классам:")
    for name, p in zip(class_names, probs.tolist()):
        bar = "█" * int(p * 30)
        print(f"  {name:>10s}: {p*100:6.2f}%  {bar}")


if __name__ == "__main__":
    main()
