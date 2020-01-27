#
# Main script for training a model for semantic segmentation of
# roads from Velodyne point clouds into Occupancy Grid Map
#
# Use the config.ini file to set the variables
# Requirements: pytorch
#
from typing import TextIO

import torch
import torch.optim as optim
import numpy as np
import torch.nn.functional as F
from torchvision import transforms
import math
from random import shuffle
import scipy.sparse

import configparser

import cv2

# our nn model.py
import model as M
from _testbuffer import ndarray
from cupshelpers.ppds import normalize
import time
from sklearn.metrics.classification import confusion_matrix


#transforms.Normalize(mean for each channel, std for each chanel)
TRANSFORMS = None #transforms.Normalize([0.0128, 0.0119, 0.0077, 0.0019, 0.0010], [0.0821, 0.0739, 0.0591, 0.0170, 0.0100])


def tensor2rgbimage(tensor):
    img = tensor.permute(1,2,0).numpy()
    height, width = img.shape[:2]
    img_map = np.zeros((width,height,3), np.uint8)
    #print(np.where((img == [1]).all(axis = 2)))
    img_map[np.where((img == [0]).all(axis = 2))] = np.array([255,120,0])
    img_map[np.where((img == [2]).all(axis = 2))] = np.array([255,255,255])
    img_map[np.where((img == [1]).all(axis = 2))] = np.array([0,0,0])

    return img_map


def load_labels_fromfile_as_img(dataset_config, size):
    for n in train_list:
        file = (dataset_config['target_path'] + n + '_label')
        numpy_file = np.fromfile(file, dtype=float)
        reshaped = numpy_file.reshape(size, size)
        img = labels_to_img(reshaped, size)
    return img


def labels_to_img(numpy_image, size):
    reshaped = numpy_image
    img = np.zeros((size, size, 3), np.uint8)
    for i in range(size):
        # print("")
        for j in range(size):
            if reshaped[i][j] == 0:
                img[i][j] = np.array([255, 120, 0])
            elif reshaped[i][j] == 1.0:
                img[i][j] = np.array([255, 255, 255])
            elif reshaped[i][j] == 2.0:
                img[i][j] = np.array([0, 0, 0])
    return img


def pred_and_label_to_img_and_confusion_matrix(numpy_image1, numpy_image2, confusion_mtx, size):
    reshaped = numpy_image1
    reshaped2 = numpy_image2
    img1 = np.zeros((size, size, 3), np.uint8)
    img2 = np.zeros((size, size, 3), np.uint8)
    
    for i in range(size):
        # print("")
        for j in range(size):
            val1 = 0
            val2 = 1
            if reshaped[i][j] == 0:
                img1[i][j] = np.array([255, 120, 0])
            elif reshaped[i][j] == 1.0:
                img1[i][j] = np.array([255, 255, 255])
                val1 = 1
            elif reshaped[i][j] == 2.0:
                img1[i][j] = np.array([0, 0, 0])
                val1 = 2
            
            if reshaped2[i][j] == 0:
                img2[i][j] = np.array([255, 120, 0])
                val2 = 0
            elif reshaped2[i][j] == 1.0:
                img2[i][j] = np.array([255, 255, 255])
            elif reshaped2[i][j] == 2.0:
                val2 = 2
                img2[i][j] = np.array([0, 0, 0])
                
            update_confusion(val1, val2, confusion_mtx)
                
    return img1, img2

def show_dataset(dataset_tensor, size):

    cv2.imshow('Max', convert_metric_map_to_image(dataset_tensor[1][0], 'max', size))
    cv2.imshow('Mean', convert_metric_map_to_image(dataset_tensor[1][1], 'mean', size))
    cv2.imshow('Min', convert_metric_map_to_image(dataset_tensor[1][2], 'min', size))
    cv2.imshow('Numb', convert_metric_map_to_image(dataset_tensor[1][3], 'numb', size))
    cv2.imshow('Std', convert_metric_map_to_image(dataset_tensor[1][4], 'std', size))
    cv2.waitKey(0)


def fixed_normalize_to_img(map, new_max_value, max_value, min, size):
    img_map = np.zeros((size, size, 3), np.uint8)

    for i in range(size):
        for j in range(size):
            new_val = map[j][i].item()
            # print(new_val)
            if new_val < min:
                img_map[j][i][0] = min
                img_map[j][i][1] = min
                img_map[j][i][2] = min
            cel_pixel = (new_val - min) * new_max_value / (max_value - min)
            img_map[j][i][0] = cel_pixel
            img_map[j][i][1] = cel_pixel
            img_map[j][i][2] = cel_pixel
    return img_map


def fixed_normalize(map, new_max_value, max_value, min_value, size):
    img_map = np.zeros((size, size))

    for i in range(size):
        for j in range(size):
            new_val = map[j][i].item()
            # print(new_val)
            if new_val < min_value:
                img_map[j][i] = min_value
            cel_pixel = (new_val - min_value) * new_max_value / (max_value - min_value)
            img_map[j][i] = cel_pixel
    return img_map


def print_values(map, size):
    for i in range(size):
        for j in range(size):
            new_val = map[j][i].item()
            print(new_val)


def get_raw_min_and_max_values_to_normalize(metric_type):
    map_min = -10.0
    if metric_type == 'max':
        map_max = 1.852193
    elif metric_type == 'numb':
        map_max = 64.0
        map_min = 0.0
    elif metric_type == 'min':
        map_max = 1.852193
    elif metric_type == 'mean':
        map_max = 1.852193
    elif metric_type == 'std':
        map_max = 15.0
        map_min = -1.0

    return map_max, map_min


def convert_raw_metric_map_to_image(metric_map, metric_type, size):
# TODO Pegar das variáveis
    map_max, map_min = get_raw_min_and_max_values_to_normalize(metric_type)
    img = fixed_normalize_to_img(metric_map, 255.0, map_max, map_min, size)
    return img


def convert_metric_map_to_image(metric_map, metric_type, size):
# TODO Pegar das variáveis
    map_max = 1
    map_min = 0
    img = fixed_normalize_to_img(metric_map, 255.0, map_max, map_min, size)
    return img


def getDatasetList(file_name):
    file = open(file_name)
    content = file.read().splitlines()
    # content_list = content.split('\n')
    # for i in content:
    #     print(i)
    return content


def file_raw_to_tensor(img_x_dim, img_y_dim, file, metric_type):
    numpy_file = np.fromfile(file, dtype=float)
    max_value, min_value = get_min_and_max_values_to_normalize(metric_type)
    reshaped = numpy_file.reshape(img_x_dim, img_y_dim)
    normalized_data = fixed_normalize(reshaped, 1.0, max_value, min_value)
    data_tensor = torch.from_numpy(normalized_data)
    return data_tensor


def file_npz_to_tensor(file):
    sparse_matrix = scipy.sparse.load_npz(file)
    normalized_data = sparse_matrix.todense()
    normalized_data = np.array(normalized_data)
    data_tensor = torch.from_numpy(normalized_data)
    
    return data_tensor


def file_to_tensor(transforms, img_x_dim, img_y_dim, file):
    numpy_file = np.fromfile(file, dtype=float)
    reshaped = numpy_file.reshape(img_x_dim, img_y_dim)
    data_tensor = torch.from_numpy(reshaped)
    if transforms != None:
        data_tensor = transforms(data_tensor)
    return data_tensor


def label_file_npz_to_tensor(file):
    sparse_matrix = scipy.sparse.load_npz(file)
    normalized_data = sparse_matrix.todense()
    normalized_data = np.array(normalized_data)

    weight = np.array([len(np.where(normalized_data == t)[0]) for t in np.unique(normalized_data)])
    data_tensor = torch.from_numpy(normalized_data)

    return data_tensor, weight


def label_file_to_tensor(img_x_dim, img_y_dim, file):
    numpy_file = np.fromfile(file, dtype=float)

    reshaped = numpy_file.reshape(img_x_dim, img_y_dim)
    weight = np.array([len(np.where(reshaped == t)[0]) for t in np.unique(reshaped)])

    data_tensor = torch.from_numpy(reshaped)

    return data_tensor, weight


def load_bach_data(transforms, dataset_list, data_path, target_path, last_element, batch_size,
                   input_dimensions, img_x_dim, img_y_dim, n_classes):
    batch_weight = np.zeros(n_classes)
    dataset_size = len(dataset_list)

    if (dataset_size - (last_element + batch_size)) <= 0:
        batch_size = (dataset_size - last_element)
        # print("sobrou: ", batch_size)

    data = torch.zeros(batch_size, input_dimensions, img_x_dim, img_y_dim)
    target = torch.zeros(batch_size, img_x_dim, img_y_dim, dtype=torch.int64)

    for j in range(batch_size):
        # + 1 se indice comeca em 1
        # print(batch_size)
        # print("lastele: ", last_element, " j:", j)

        data[j][0] = (file_npz_to_tensor(data_path + str(dataset_list[last_element]) + '_max.npz'))
        data[j][1] = (file_npz_to_tensor(data_path + str(dataset_list[last_element]) + '_mean.npz'))
        data[j][2] = (file_npz_to_tensor(data_path + str(dataset_list[last_element]) + '_min.npz'))
        data[j][3] = (file_npz_to_tensor(data_path + str(dataset_list[last_element]) + '_numb.npz'))
        data[j][4] = (file_npz_to_tensor(data_path + str(dataset_list[last_element]) + '_std.npz'))
        
        if transforms != None:
            data[j] = transforms(data[j])
            
        tmp, new_weights = label_file_npz_to_tensor((target_path + str(dataset_list[last_element]) + '_label.npz'))
        target[j] = tmp
        last_element = last_element + 1
        batch_weight = batch_weight + new_weights
        max_weight = max(batch_weight)
        batch_weight = max_weight / batch_weight

#         cv2.imshow('window', labels_to_img(target[j].numpy(), img_x_dim))
#         cv2.waitKey(0)
        # batch_weight = batch_weight + new_weights
    # cv2.imshow('Max', convert_metric_map_to_image(data[0][0], 'max', size))
    # max_weight = max(batch_weight)
    # batch_weight = max_weight / batch_weight
    # normalize weights
    # max_w = max(batch_weight)
    # min_w = min(batch_weight)
    # batch_weight = (batch_weight - 1)*10/(max_w - 1) + 1
    # print(weights)
    # ziped = list(zip(dataset, weights))
    # shuffle(ziped)
    # dataset, weights = zip(*ziped)
    return data, target, last_element, batch_weight

def update_confusion(y, x, conf):
    conf[y][x] = conf[y][x] + 1


def test(model, device, test_list, epoch, batch_size, dataset_config, dnn_config, n_classes, img_width, img_height, input_channels):
    global TRANSFORMS
    confusion_mtx = np.array([[0, 0, 0],[0, 0, 0],[0, 0, 0]])
    print("Testing!")
    model.eval()
    test_loss = 0
    correct = 0
    last_element = 0
    with torch.no_grad():
        for batch_idx in range(math.floor(len(test_list)/batch_size)):
            if last_element < (len(test_list)):
                data, target, last_element, weights = load_bach_data(TRANSFORMS, test_list, dataset_config['test_path'],
                                                            dataset_config['target_path'],
                                                            last_element, batch_size, input_channels,
                                                            img_width, img_height, n_classes)
                data = data.to(device)
                target = target.long().to(device)
                output, prob_softmax = model(data)
                batch_weight = torch.FloatTensor(weights).cuda()
                test_loss += F.cross_entropy(output, target, reduction="sum").item() # sum up batch loss
                pred = prob_softmax.max(1, keepdim=True)[1] # get the index of the max log-probability
                correct += pred.eq(target.view_as(pred)).sum().item()
                imgPred = pred[0]
                imgPred = imgPred.cpu().float()
                target = target.cpu().float()
                im1, im2 = pred_and_label_to_img_and_confusion_matrix(imgPred[0].numpy(), target[0].numpy(), confusion_mtx, img_width)
                cv2.imwrite(dnn_config['save_log_files'] + 'img' + str(batch_idx) + '.png', im1)
                cv2.imwrite(dnn_config['save_log_files'] + 'label' + str(batch_idx) + '.png', im2)
        #    test_loss /= len(test_loader.dataset)
        test_loss /= (len(test_list)*batch_size*img_width*img_height)
        texto = ('Test set: epoch {} Average loss {:.10f}, Accuracy: {}/{} {:.6f}\n'.format(epoch,
            test_loss, correct, len(test_list)*img_width*img_height,
            correct / (len(test_list)*img_width*img_height)))
        texto_matrix = ("Matriz de confusao final:")
        print(confusion_mtx)
        print(texto)
        np.savetxt(dnn_config['save_log_files']+'test_confusion.txt', confusion_mtx)
        arq = open(dnn_config['save_log_files']+'Final_Test_averange.txt', 'a')
        arq.write(texto)
        arq.write(texto_matrix)
        arq.close()
        
    
if __name__ == '__main__':

    # TODO Fazer um função pra isso:
    # Load config and initializations
    config_file = configparser.ConfigParser()
    config_file.read('config_test.ini')
    dataset_config = config_file['DATASET']
    dnn_config = config_file['DNN']

    test_list = getDatasetList(dataset_config['test_list'])
    #TODO Escrever no parametro [max_normalize_std] e max_normalize_numb, os valores pra esse dataset

    batch_size = int(dnn_config['batch_size'])
    input_channels = int(dataset_config['channels'])
    n_classes = int(dnn_config['classes'])
    img_width = int(dataset_config['img_width'])
    img_height = int(dataset_config['img_height'])
    use_cuda = int(dnn_config['use_cuda'])
    device_number = dnn_config['device']
    interval_save_model = int(dnn_config['interval_save_model'])
    random_seed = int(dnn_config['manual_seed'])
    dropout_prob = float(dnn_config['dropout_prob'])
    # CUDA_LAUNCH_BLOCKING = 1

    use_cuda = use_cuda and torch.cuda.is_available()
    # TODO: Arrumar pra escolher pelo parametro
    device = torch.device("cuda") # torch.device(("cuda:" + device_number) if use_cuda else "cpu")
    print(torch.cuda.current_device(), torch.cuda.get_device_capability(int(device_number)))
    print('Using Device: ', device)
    
    if dataset_config.getboolean('shuffle_data'):
        shuffle(train_list)
        shuffle(test_list)
        print('Data Shuffled')

    # load model
    model = M.FCNN(n_input=input_channels, n_output=n_classes, prob_drop=dropout_prob).to(device)
    print('Model loaded', model)

    if dnn_config['use_trained_model'] is not "":
        model.load_state_dict(torch.load(dnn_config['use_trained_model']))
        print("Using model: ", dnn_config['use_trained_model'])

    print('Iniciando Test \n')

    inicio = time.time()
    epoch = 1
    test(model, device, test_list, epoch, batch_size, dataset_config, dnn_config, n_classes, img_width, img_height, input_channels)
    print("\nTempo total 1 epoca: ", time.time() - inicio)
