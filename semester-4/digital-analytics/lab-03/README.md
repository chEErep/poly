# Классификация изображений — лабораторная 3

Цифровая аналитика, 4 семестр. Модель на основе ResNet-18 обучается различать фотографии людей и лошадей. Для transfer learning используются веса ImageNet, последний слой заменён на классификатор двух классов.

## Запуск

Нужен Python 3.10 или новее. Из этой папки:

```bash
python -m venv .venv
```

Активируйте окружение (`.venv\Scripts\activate` в Windows или `source .venv/bin/activate` в Linux) и установите зависимости:

```bash
pip install -r requirements.txt
```

Для CPU-версии PyTorch сначала можно установить `torch` и `torchvision` через индекс `https://download.pytorch.org/whl/cpu`. При наличии CUDA устройство выбирается автоматически.

Готовые веса находятся в `weights.pth`. Проверка отдельного изображения:

```bash
python predict.py --image path/to/image.jpg --weights weights.pth
```

Скрипт выводит класс и значения softmax. Эти значения не являются гарантией правильности ответа. Модель обучалась на двух классах; произвольных животных и другие объекты она отдельно не распознаёт.

## Обучение

Ожидается структура `data/train/horses`, `data/train/humans`, `data/val/horses`, `data/val/humans`:

```bash
python train.py --train_dir data/train --val_dir data/val --epochs 10
```

Без `--val_dir` данные делятся по классам в соотношении около 80/20 с seed 42. Случайные преобразования применяются только к train; для val используются resize и нормализация. Лучший checkpoint сохраняется по accuracy на val.

`download_dataset.py` скачивает Horses or Humans с Kaggle. Для него отдельно нужен `pip install kagglehub==1.0.2`. Существующие папки данных он не перезаписывает. Датасет и пользовательские изображения в репозиторий не включены.

Проверки разделения выборки и преобразований:

```bash
pip install -r requirements-dev.txt
python -m pytest -q
```
