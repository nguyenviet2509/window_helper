---
phase: 08
title: "Labeling + YOLOv8n training pipeline"
status: pending
priority: P2
effort: 1.5d
blockers: [07]
---

## Context
Dataset từ Phase 7 cần label bounding box quanh mob (class `mob` + optional `player`/`npc` để filter).

## Goal
Train YOLOv8n model phát hiện mob → export ONNX.

## Files (offline tools, không tích hợp app)
- New: `tools/labeling/README.md` (workflow doc)
- New: `tools/labeling/train.py` (Colab/local training script)
- New: `tools/labeling/dataset.yaml` (Ultralytics format)
- New: `tools/labeling/export-onnx.py`
- Output: `models/mob-detector.onnx` (ship riêng, không bake vào binary)

## Steps

### 1. Labeling
- Tool: LabelImg (offline) HOẶC Roboflow (online, free tier 1000 images).
- Classes: `mob` (chính), `player`, `npc` (optional filters).
- Target: 300-500 labeled frames đa dạng map/mob.
- Format export: YOLO TXT (1 file/image, `class cx cy w h` normalized).

### 2. Dataset structure
```
dataset/
  images/
    train/  *.png
    val/    *.png  (20% split)
  labels/
    train/  *.txt
    val/    *.txt
  dataset.yaml
```

`dataset.yaml`:
```yaml
path: ./dataset
train: images/train
val: images/val
names:
  0: mob
  1: player
  2: npc
```

### 3. Training script `train.py`
```python
from ultralytics import YOLO
model = YOLO("yolov8n.pt")  # pretrained, ~6MB
model.train(
    data="dataset.yaml",
    epochs=80,
    imgsz=640,
    batch=16,
    device=0,        # GPU
    augment=True,
    hsv_h=0.02, hsv_s=0.5, hsv_v=0.4,
    fliplr=0.5,
    mosaic=1.0,
)
```

- Train trên Colab GPU (free T4).
- Augmentation quan trọng: HSV jitter cho lighting variance, fliplr cho directional invariance.
- Validation mAP target ≥0.7 @ IoU 0.5.

### 4. Export ONNX
```python
model = YOLO("runs/detect/train/weights/best.pt")
model.export(format="onnx", imgsz=640, opset=12, simplify=True)
```

Output: `best.onnx` (~12MB), rename `models/mob-detector.onnx`.

### 5. Validation
- Test set 50 frames không thấy lúc train.
- mAP, precision, recall per class.
- Visual sanity check: overlay bbox lên 10 frames.

### 6. Iteration
- Nếu mAP <0.5 → label thêm 200 frames vùng yếu → retrain.
- Lưu version: `mob-detector-v1.onnx`, `v2.onnx`, ...

## Success Criteria
- mAP@0.5 ≥ 0.7 trên val set.
- Recall mob class ≥ 0.8 (don't miss many mobs).
- Precision mob class ≥ 0.7 (few false positives).
- ONNX file <20MB.

## Risks
- Class imbalance (player/npc rare) → focal loss / oversampling.
- Domain shift map khác → cần re-train per map nếu performance drop.

## Open Questions
- Có nên train 1 model multi-map hay 1 model per map? Khuyến nghị: 1 multi-map, retrain khi accuracy drop.
