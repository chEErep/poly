"""
model.py
--------
Описание модели для задачи бинарной классификации "человек / лошадь".

За основу взята предобученная нейросеть ResNet-18 из библиотеки torchvision.
Она уже обучена на огромном датасете ImageNet (1000 классов), поэтому её
свёрточные слои хорошо умеют выделять базовые признаки изображений
(края, текстуры, формы). Нам остаётся только заменить её последний
полносвязный слой так, чтобы на выходе было 2 класса, а не 1000.
"""

import torch.nn as nn
from torchvision import models
from torchvision.models import ResNet18_Weights


def build_model(num_classes: int = 2, pretrained: bool = True) -> nn.Module:
    """
    Создаёт модель ResNet-18 для бинарной классификации.

    Параметры:
        num_classes (int): количество классов на выходе. По умолчанию 2
            (человек / лошадь).
        pretrained (bool): использовать ли веса, предобученные на ImageNet.
            При обучении ставим True, при инференсе (predict.py) — False,
            потому что свои веса мы и так загрузим из файла.

    Возвращает:
        nn.Module: готовая модель ResNet-18 с заменённой "головой".
    """
    # Загружаем архитектуру ResNet-18.
    # Если pretrained=True — подтягиваем веса, обученные на ImageNet,
    # это сильно ускоряет обучение и повышает точность (transfer learning).
    if pretrained:
        weights = ResNet18_Weights.DEFAULT
    else:
        weights = None

    model = models.resnet18(weights=weights)

    # У ResNet-18 последний слой называется fc (fully connected, полносвязный).
    # По умолчанию это Linear(in_features=512, out_features=1000),
    # т.к. ImageNet содержит 1000 классов.
    # Нам нужно 2 класса — поэтому заменяем этот слой на новый.
    in_features = model.fc.in_features          # размер входа = 512
    model.fc = nn.Linear(in_features, num_classes)

    return model


# Если запустить файл напрямую (python model.py),
# выведем структуру модели и количество параметров — для проверки.
if __name__ == "__main__":
    net = build_model(num_classes=2, pretrained=False)
    print(net)

    total_params = sum(p.numel() for p in net.parameters())
    trainable_params = sum(p.numel() for p in net.parameters() if p.requires_grad)
    print(f"\nВсего параметров:        {total_params:,}")
    print(f"Из них обучаемых:        {trainable_params:,}")
