import sys
import os
import math
import cv2
import matplotlib.pyplot as plt

def read_groud_truth_points(gt_dir, gt_file_name):
	#print (gt_dir + gt_file_name)
	
	gt_file = open(gt_dir + gt_file_name, "r")
	file_content = gt_file.readlines()
	
	gt_points_list = []
	count = 0
	gt_points_left = []
	gt_points_right = []
	for line in file_content:
		line = line.strip().split()
		line[0] = int(line[0])
		line[1] = int(line[1])
		if count < 4:
			gt_points_left.append(line[:])
		else:
			gt_points_right.append(line[:])
		count = count + 1
		#print(gt_points_right)
	gt_points_list.append(gt_points_left[:])
	gt_points_list.append(gt_points_right[:])
	#print(gt_points_list)
	return gt_points_list

def transform_groud_truth_points_to_yolo(gt_points_list):
	yolo_points_list_left = []
	yolo_points_list_right = []
	yolo_points_list = []
	points = []
	for i in range(len(gt_points_list[0]) - 1):
		points.append(float((gt_points_list[0][i + 1][0] + gt_points_list[0][i][0])) / (2.0 * 640.0))
		points.append(float((gt_points_list[0][i + 1][1] + gt_points_list[0][i][1])) / (2.0 * 480.0))
		points.append(abs(float((gt_points_list[0][i + 1][1] - gt_points_list[0][i][1])) / 480.0))
		points.append(abs(float((gt_points_list[0][i + 1][0] - gt_points_list[0][i][0])) / 640.0))
		yolo_points_list_left.append(points[:])
		del points[:]
	
	for i in range(len(gt_points_list[1]) - 1):
		points.append(float((gt_points_list[1][i + 1][0] + gt_points_list[1][i][0])) / (2.0 * 640.0))
		points.append(float((gt_points_list[1][i + 1][1] + gt_points_list[1][i][1])) / (2.0 * 480.0))
		points.append(abs(float((gt_points_list[1][i + 1][1] - gt_points_list[1][i][1])) / 480.0))
		points.append(abs(float((gt_points_list[1][i + 1][0] - gt_points_list[1][i][0])) / 640.0))
		yolo_points_list_right.append(points[:])
		del points[:]
	
	yolo_points_list.append(yolo_points_list_left[:])
	yolo_points_list.append(yolo_points_list_right[:])
	return yolo_points_list

def transform_yolo_points_groundtruth_to_coordinates(gt_dir, gt_file_name):
	points = []
	gt_points_list = []
	gt_points_list_left = []
	gt_points_list_right = []
	gt_file = open(gt_dir + gt_file_name, "r")
	file_content = gt_file.readlines()
	
	i = 0
	for line in file_content:
		line = line.strip().split()
		if (i == 0):
			points.append(int((float(line[0]) + (float(line[2]) / 2)) * 640.0))
			points.append(int((float(line[1]) - (float(line[3]) / 2)) * 480.0))
			gt_points_list_left.append(points[:])
		del(points[:])
		i = i + 1
		points.append(int((float(line[0]) - (float(line[2]) / 2)) * 640.0))
		points.append(int((float(line[1]) + (float(line[3]) / 2)) * 480.0))
		gt_points_list_left.append(points[:])
		del(points[:])
		if (i == 3):
			break
	
	i = 0
	for line in file_content:
		line = line.strip().split()
		if (i == 3):
			points.append(int((float(line[0]) - (float(line[2]) / 2)) * 640.0))
			points.append(int((float(line[1]) - (float(line[3]) / 2)) * 480.0))
			gt_points_list_right.append(points[:])
		del (points[:])
		i = i + 1
		if (i > 3):
			points.append(int((float(line[0]) + (float(line[2]) / 2)) * 640.0))
			points.append(int((float(line[1]) + (float(line[3]) / 2)) * 480.0))
			gt_points_list_right.append(points[:])
			del (points[:])
		
	gt_points_list.append(gt_points_list_left[:])
	gt_points_list.append(gt_points_list_right[:])
	return gt_points_list

def dist(x1, y1, x2, y2):
	dx = float(x1) - float(x2)
	dy = float(y1) - float(y2)
	return math.sqrt((dx * dx) + (dy * dy))


def read_and_convert_4_points_coordinates(predictions_dir, gt_file_name, image_width, image_heigth):
	print (predictions_dir + gt_file_name)
	predictions_files_list = open(predictions_dir + gt_file_name, "r")
	content = predictions_files_list.readlines()
	
	prediction = []
	predictions_list = []
	predictions_list_left = []
	predictions_list_right = []
	xmin = 999999
	xmax = 0
	for line in content:
		line = line.replace('\n', '').rsplit(' ')
		if (int((float(line[1]) - (float(line[3]) / 2)) * image_width) < xmin):
			xmin = int((float(line[1]) - (float(line[3]) / 2)) * image_width)
		if (int((float(line[1]) + (float(line[3]) / 2)) * image_width) > xmax):
			xmax = int((float(line[1]) + (float(line[3]) / 2)) * image_width) 
    
	#print ("xmin = ",xmin)
	#print ("xmax = ",xmax)
	
	for line in content:
		line = line.replace('\n', '').rsplit(' ')
		if (int(float(line[1]) * image_width) > xmin + (xmax - xmin) / 2):
			prediction.append(int((float(line[1]) + (float(line[3]) / 2)) * image_width))
			prediction.append(int((float(line[2]) + (float(line[4]) / 2)) * image_heigth))
			prediction.append(int((float(line[1]) - (float(line[3]) / 2)) * image_width))
			prediction.append(int((float(line[2]) - (float(line[4]) / 2)) * image_heigth))	
			predictions_list_right.append(prediction[:])
		else:
			prediction.append(int((float(line[1]) - (float(line[3]) / 2)) * image_width))
			prediction.append(int((float(line[2]) + (float(line[4]) / 2)) * image_heigth))
			prediction.append(int((float(line[1]) + (float(line[3]) / 2)) * image_width))
			prediction.append(int((float(line[2]) - (float(line[4]) / 2)) * image_heigth))
			predictions_list_left.append(prediction[:])
		#print(predictions_list)
		del prediction[:]

	predictions_list.append(predictions_list_left[:])
	predictions_list.append(predictions_list_right[:])
	#print(predictions_list)

	return predictions_list


def find_image_path():
	for i, param in enumerate(sys.argv):
		if param == '-show':
			return sys.argv[i + 1]
	return ""


def show_image(gt_points, predictions_points, predictions_points_2, gt_file_path, images_path):	
	img = cv2.imread(images_path + gt_file_name.replace('txt', 'png'))
	print(images_path)
	
	
	#imprime os pontos do groundthruth em linhas
	for i in range(len(gt_points)):
		for j in (range(len(gt_points[i]) - 1)):
			tuple_of_points_0 = (int(gt_points[i][j][0]), int(gt_points[i][j][1]))
			tuple_of_points_1 = (int(gt_points[i][j+1][0]), int(gt_points[i][j+1][1]))
			cv2.line(img, tuple_of_points_0, tuple_of_points_1, (255,0,0), 1)
			j = j + 1
	
	#imprime os bound boxes da yolo
	for i in range(len(predictions_points[0])):
		tuple_of_points_0 = (int(predictions_points[0][i][0]), int(predictions_points[0][i][1]))
		tuple_of_points_1 = (int(predictions_points[0][i][2]), int(predictions_points[0][i][3]))
		cv2.rectangle(img, tuple_of_points_0, tuple_of_points_1, (255, 0, 255), 2)
	
	for i in range(len(predictions_points[1])):
		tuple_of_points_0 = (int(predictions_points[1][i][0]), int(predictions_points[1][i][1]))
		tuple_of_points_1 = (int(predictions_points[1][i][2]), int(predictions_points[1][i][3]))
		cv2.rectangle(img, tuple_of_points_0, tuple_of_points_1, (255, 0, 255), 2)


	
	#imprime os pontos do ELAS em circulos
	for i in range(len(predictions_points_2[0])):
		tuple_of_points_0 = (int(predictions_points_2[0][i][0]), int(predictions_points_2[0][i][1]))
		tuple_of_points_1 = (int(predictions_points_2[0][i][2]), int(predictions_points_2[0][i][3]))
		cv2.circle(img, tuple_of_points_0, 1, (0, 255, 255), 5)
		cv2.circle(img, tuple_of_points_1, 1, (0, 255, 255), 5)
	
	for i in range(len(predictions_points_2[1])):
		tuple_of_points_0 = (int(predictions_points_2[1][i][0]), int(predictions_points_2[1][i][1]))
		tuple_of_points_1 = (int(predictions_points_2[1][i][2]), int(predictions_points_2[1][i][3]))
		cv2.circle(img, tuple_of_points_0, 1, (0, 255, 255), 5)
		cv2.circle(img, tuple_of_points_1, 1, (0, 255, 255), 5)

	#imprime os pontos da yolo em circulos
	for i in range(len(predictions_points[0])):
		tuple_of_points_0 = (int(predictions_points[0][i][0]), int(predictions_points[0][i][1]))
		tuple_of_points_1 = (int(predictions_points[0][i][2]), int(predictions_points[0][i][3]))
		cv2.circle(img, tuple_of_points_0, 1, (0, 0, 255), 2)
		cv2.circle(img, tuple_of_points_1, 1, (0, 0, 255), 2)
	
	for i in range(len(predictions_points[1])):
		tuple_of_points_0 = (int(predictions_points[1][i][0]), int(predictions_points[1][i][1]))
		tuple_of_points_1 = (int(predictions_points[1][i][2]), int(predictions_points[1][i][3]))
		cv2.circle(img, tuple_of_points_0, 1, (0, 0, 255), 2)
		cv2.circle(img, tuple_of_points_1, 1, (0, 0, 255), 2)	
	
	while (1):
		cv2.imshow('Lane Detector Compute Error', img)
		key = cv2.waitKey(0) & 0xff
		
		if key == 10 or key == 13:      # Enter key
			#cv2.imwrite(gt_file_name.replace('txt', 'png'), img)
			break
		elif key == 27:    # ESC key
			sys.exit()


def distance_point_line(p1, p2, p):
	dy = float(p2[1]) - float(p1[1])
	dya = float(p[1]) - float(p1[1])
	
	pdya = dya / dy
	
	x = float(p1[0]) + ((float(p2[0]) - float(p1[0])) * pdya)
	
	error = abs(int(x) - p[0])
	
	return error, x


def distance_point_line2(p1, p2, p):
	x1 = p1[0]
	y1 = p1[1]
	x2 = p2[0]
	y2 = p2[1]
	ya = p[1]
	
	num = (x2 * (y1 - ya)) + (x1 * (ya - y2))
	den = y1 - y2
	
	x = float(num) / float(den)
	x = int(x)
	
	distance = abs(x - p[0])
	
	return distance, x


def comppute_point_to_line_error(line_p1, line_p2, predictions_points):
	min_dist = 999999
	distance = 0.0
	aux = 0
	point = []
	distance = dist(line_p1[0], line_p1[1], predictions_points[0], predictions_points[1])

	if distance < min_dist:
		aux = 0
		min_dist = distance

	distance = dist(line_p2[0], line_p2[1], predictions_points[2], predictions_points[3])
	

	if distance < min_dist:
		aux = 1
		min_dist = distance
	if aux == 0:
		point.append(predictions_points[0])
		point.append(predictions_points[1])
	else:
		point.append(predictions_points[2])
		point.append(predictions_points[3])
	returned = distance_point_line2(line_p1, line_p2, point)
	point[0] = returned[1]
	
	return returned[0], point

def compute_error(gt_points, predictions_points, aux):
	error = 0
	chosen_points_list = []

	for i in (range(len(gt_points[0]) - 1)):
		for j in (range(len(predictions_points[0]))):
			returned = comppute_point_to_line_error(gt_points[0][i], gt_points[0][i+1], 
				predictions_points[0][j])
			error += returned[0]
			chosen_points_list.append(returned[1])
	
	for i in (range(len(gt_points[1]) - 1)):
		for j in (range(len(predictions_points[1]))):
			returned = comppute_point_to_line_error(gt_points[1][i], gt_points[1][i + 1], 
				predictions_points[1][j])
			error += returned[0]
			chosen_points_list.append(returned[1])
	if aux == 0:
		if (float(error) < 500.0):
			print ('Error Yolo: ' + str(error))
	else:
		if (float(error) < 500.0):
			print ('Error ELAS: ' + str(error))
	return error, chosen_points_list


if __name__ == "__main__":
	#if len(sys.argv) < 5 or len(sys.argv) > 9:
		#print ("\nUse: python", sys.argv[0], "ground_truth_dir predictions1_dir predictions2_dir image_width image_heigth -show images_path (optional) -format jpg (optional)\n")
	#else:
		if not sys.argv[1].endswith('/'):
			sys.argv[1] += '/'
		if not sys.argv[2].endswith('/'):
			sys.argv[2] += '/'
		if not sys.argv[3].endswith('/'):
			sys.argv[3] += '/'
			
		image_width  = 640 #640 #int(sys.argv[4])
		image_heigth = 480 #480 #int(sys.argv[5])
		images_path = sys.argv[1]
		images_path = images_path.split('/')
		images_path = str(images_path[- 2])
		images_path = "/media/marcelo/edae3a16-98bf-41d6-ac90-bf57de7594c3/Base_Rodrigo/" + images_path + "/images/"
		gt_files_list = [l for l in os.listdir(sys.argv[2])]
		gt_files_list.sort()
		yolo_path = sys.argv[2]
		yolo_path_split = yolo_path.split('/')
		number_of_folder = int(yolo_path_split[-3])
		#print(number_of_folder)
		#print(yolo_path_split[-2])
		yolo_path = ""
		for i in range(len(yolo_path_split) - 3):
			#if  i == 0:
			#	i = i +1
			yolo_path =  yolo_path + "/" + str(yolo_path_split[i])
		base_yolo = yolo_path
		#print(yolo_path)
		yolo_path = yolo_path + "/" + str(number_of_folder)
		yolo_path = yolo_path + "/" + str(yolo_path_split[-2]) + "/"
		#print(yolo_path)
		error = 0
		error_elas = 0
		cont = 0
		error_array = []
		array_numbers = []
		for i in range(20):
			for gt_file_name in gt_files_list:
				if not gt_file_name.endswith('.txt'):
					continue
				gt_points = read_groud_truth_points(sys.argv[1], gt_file_name)
				#if (int(number_of_folder) == 9000):
					#continue
				#gt_points_yolo = transform_groud_truth_points_to_yolo(gt_points)
				
				#print (gt_points_yolo)

				#gt_points_true = transform_yolo_points_groundtruth_to_coordinates(sys.argv[1], gt_file_name)

				#print (gt_points_true)
				predictions_points = read_and_convert_4_points_coordinates(yolo_path, gt_file_name, image_width, image_heigth)
				
				predictions_points_2 = read_and_convert_4_points_coordinates(sys.argv[3], gt_file_name, image_width, image_heigth)
				
				#print (predictions_points)
				#returned = compute_error(gt_points_true, predictions_points)
				returned = compute_error(gt_points, predictions_points, 0)
				returned_2 = compute_error(gt_points, predictions_points_2, 1)
				if (float(returned[0]) < 500.0):
					error += returned[0]
				if (float(returned[0]) > 500.0):
					continue
				cont += 1
				if (int(number_of_folder) == 20000 and float(returned_2[0]) < 500.0):
					error_elas += returned_2[0]
				#if images_path:
					#show_image(gt_points, predictions_points, returned[1], returned_2[1], gt_file_name, images_path)
				#show_image(gt_points, predictions_points, predictions_points_2, gt_file_name, images_path)
			#print(number_of_folder)
			print ('TOTAL Error Yolo: ' + str(error/cont))
			print ('TOTAL Error ELAS: ' + str((error_elas * 20)/cont))
			error_array.append(int(error/cont))
			array_numbers.append(int(number_of_folder))
			number_of_folder = number_of_folder + 1000
			yolo_path = base_yolo
			yolo_path = yolo_path + "/" + str(number_of_folder)
			yolo_path = yolo_path + "/" + str(yolo_path_split[-2]) + "/"
		plt.plot(array_numbers, error_array)
		plt.xlabel('Número da pasta')
		plt.ylabel('TOTAL Error')
		titulo = "Error da pasta " + yolo_path_split[-2]
		plt.title(str(titulo))
		plt.show()
		
