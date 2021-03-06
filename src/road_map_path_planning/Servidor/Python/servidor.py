import socket
import os


#HOST = '10.42.0.25'
#HOST = '192.168.108.78'
HOST = ([l for l in ([ip for ip in socket.gethostbyname_ex(socket.gethostname())[2]if not ip.startswith("127.")][:1], [[(s.connect(('8.8.8.8', 53)),s.getsockname()[0], s.close()) for s in [socket.socket(socket.AF_INET,socket.SOCK_DGRAM)]][0][1]]) if l][0][0])

PORT = 8000
rddf = os.environ['CARMEN_HOME']

def TrataRDDF(rddf):
    filename = rddf+"/data/rddf_annotation_log_20140418.txt"
    lista = []
    with open(filename) as f:
        content = f.readlines()
    for line in content:
        if(line[:10] == "RDDF_PLACE"): 
            li=line.split()
            lista.append(li)

    for x in range(len(lista)):
       lista[x][0] = lista[x][0][11:].replace("_"," ")
    return lista

def recebida(mensagem):
    mensagem = mensagem.split(";;")
    print("RDDF_PLACE_"+mensagem[0].replace(" ","_"))    
    print("RDDF_PLACE_"+mensagem[1].replace(" ","_"))

def cancelada(mensagem):
    mensagem = mensagem.split(";;")
    print("RDDF_PLACE_"+mensagem[0].replace(" ","_"))    
    print("RDDF_PLACE_"+mensagem[1].replace(" ","_"))





listageral = TrataRDDF(rddf)
#print listageral
tratado = ''.join(str(x) for x in listageral)
tratado = tratado[1:-1].replace("][",";;").replace("'","")
#print tratado
print("Aguardando Conexao: ")
tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
orig = (HOST, PORT)
tcp.bind(orig)
tcp.listen(1)

while True:
    con, cliente = tcp.accept()
#    print('Conectado por', cliente)
    while True:
        msg = con.recv(1024)
        if not msg:
            break
#        print(cliente, msg)
        if(msg[:5] == "First"):
            con.send(tratado)
            print("Cliente ",msg[5:]," recebeu os locais.")
        elif(msg[:3]=="666"):
            con.send("Requisicao enviada com sucesso!")
            recebida(msg[4:])
            print("Requisicao recebida: ",msg[4:])
        elif(msg[:3]=="999"):
            con.send("Requisicao cancelada!") 
            cancelada(msg[4:])
            print("Requisicao cancelada: ",msg[4:])

        else:
            con.send("Socket nao planejado: "+msg)
#    print 'Finalizando conexao do cliente', cliente
    con.close()




