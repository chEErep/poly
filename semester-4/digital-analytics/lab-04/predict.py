"""Классификация байтов изображения моделью из лабораторной 3."""
import argparse
import io
import os
import warnings
from pathlib import Path
from threading import Lock

import torch
from PIL import Image
from torchvision import transforms
from model import build_model

DEFAULT_WEIGHTS = Path(os.environ.get('WEIGHTS_PATH', Path(__file__).resolve().parent.parent / 'lab-03/weights.pth'))
_cache = None
_lock = Lock()


def _load_model(weights_path=DEFAULT_WEIGHTS):
    global _cache
    path = Path(weights_path).resolve()
    with _lock:
        if _cache is not None and _cache[0] == path:
            return _cache[1:]
        device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
        checkpoint = torch.load(path, map_location='cpu', weights_only=True)
        names = checkpoint.get('class_names')
        if not isinstance(names, list) or len(names) != 2 or not all(isinstance(n, str) for n in names):
            raise ValueError('В checkpoint должны быть два имени класса')
        model = build_model(num_classes=len(names), pretrained=False)
        model.load_state_dict(checkpoint['model_state_dict'])
        model.to(device).eval()
        _cache = (path, model, names, device)
        return model, names, device


def predict_image(image_bytes: bytes, weights_path=DEFAULT_WEIGHTS) -> str:
    if not image_bytes or len(image_bytes) > 8 * 1024 * 1024:
        raise ValueError('Изображение должно занимать от 1 байта до 8 MiB')
    with warnings.catch_warnings():
        warnings.simplefilter('error', Image.DecompressionBombWarning)
        with Image.open(io.BytesIO(image_bytes)) as source:
            if source.width * source.height > 16_000_000:
                raise ValueError('Изображение превышает 16 мегапикселей')
            image = source.convert('RGB')
    transform = transforms.Compose([
        transforms.Resize((224, 224)), transforms.ToTensor(),
        transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225]),
    ])
    model, names, device = _load_model(weights_path)
    with torch.inference_mode():
        result = model(transform(image).unsqueeze(0).to(device))
    return names[int(result.argmax(dim=1).item())]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Классификация изображения')
    parser.add_argument('--image', required=True)
    parser.add_argument('--weights', default=DEFAULT_WEIGHTS)
    args = parser.parse_args()
    path = Path(args.image)
    if path.stat().st_size > 8 * 1024 * 1024:
        parser.error('Файл превышает 8 MiB')
    print(predict_image(path.read_bytes(), args.weights))
