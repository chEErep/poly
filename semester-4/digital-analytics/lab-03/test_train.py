from pathlib import Path
from PIL import Image
import pytest
import torch
from torchvision import transforms
from train import build_dataloaders


def dataset(root, names=('horses','humans')):
    for name in names:
        folder=root/name
        folder.mkdir(parents=True)
        for i in range(5):
            Image.new('RGB',(32,32),(i*30,80,150)).save(folder/f'{i}.png')


def test_split_has_separate_transforms_and_no_overlap(tmp_path):
    dataset(tmp_path)
    train,val,names=build_dataloaders(str(tmp_path),None,2)
    assert names==['horses','humans']
    assert set(train.dataset.indices).isdisjoint(val.dataset.indices)
    assert len(train.dataset)+len(val.dataset)==10
    assert any(isinstance(t,transforms.RandomRotation) for t in train.dataset.dataset.transform.transforms)
    assert not any(isinstance(t,transforms.RandomRotation) for t in val.dataset.dataset.transform.transforms)
    assert torch.equal(val.dataset[0][0],val.dataset[0][0])


def test_mismatched_classes_rejected(tmp_path):
    dataset(tmp_path/'train')
    dataset(tmp_path/'val',('cats','humans'))
    with pytest.raises(ValueError,match='совпадать'):
        build_dataloaders(str(tmp_path/'train'),str(tmp_path/'val'),2)


def test_invalid_batch_size(tmp_path):
    with pytest.raises(ValueError,match='batch_size'):
        build_dataloaders(str(tmp_path),None,0)
