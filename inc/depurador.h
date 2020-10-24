
/*
@author : SPC SOEO - Proyecto SM13

Nombre funci�n : depurador
Retorna : -

Par�metros : S : Severidad(nivel de test).Variable entera[0, 5] que le indica a
esta funci�n el nivel de penetraci�n en el c�digo.
0 : depurador desactivado
1 : severidad del depurador global
2 : severidad media
3 : severidad particular(ideal para test unitario)
SM : nombre del sub - m�dulo llamante
Texto : mensaje de informaci�n

Descripci�n : Esta funci�n permite depurar el ME a diferentes
niveles de penetraci�n del c�digo(desde lo global
    hacia lo m�s particular) mediante la impresi�n en
    consola de diferentes mensaje de informaci�n.

*/


#ifndef depurador_H
#define depurador_H

#include <stdint.h>

#define depurador(eS,sTexto) \
if (eS>= eSeveridad) printf("\n\t ---------------------------------------------- \n At time %lu - %s %d\n\t %s \n",xTaskGetTickCount(),__FILE__, __LINE__,  sTexto)

#define depuradorN(eS, sTexto, iValor) \
if (eS>= eSeveridad) printf("\n\t ---------------------------------------------- \n At time %lu - %s %d\n\t %s %d \n",xTaskGetTickCount(),__FILE__, __LINE__,  sTexto, iValor)

typedef enum {
    eDebug = 0,  
    eInfo, 
    eWarn,
    eError
}severidad_t;

severidad_t eSeveridad;



#pragma once

#endif // !depurador_H




