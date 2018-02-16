qlua ./run.lua --dataset kitti --datapath /dados/kitti-road-dataset/ --model models/road_model.lua --save save/trained/model/ --imHeight 400 --imWidth 200 --labelHeight 400 --labelWidth 200 --cachepath KITTI_DATASET_REDUCED --batchSize 1 --channels 5 --learningRate 0.0006 --lrDecayEvery 0 --maxepoch 200
