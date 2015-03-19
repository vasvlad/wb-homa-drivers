wb-homa-drivers
===============

MQTT drivers for compatible with HomA conventions
    * Kernel 1-wire interface driver (currently thermometers only)
    * GPIO-drivern switches driver
    * Ninja Cloud (ninjablocks.com) bridge


Lanuage: C++11, g++-4.7 is required




wb-homa-modbus
==============

Драйвер Modbus запускается командой `/etc/init.d/wb-homa-modbus start`
По умолчанию запуск драйвера происходит при загрузке системы при
наличии конфигурационного файла `/etc/wb-homa-modbus.conf`. При
установке пакета создаётся пример конфигурационного файла -
`/etc/wb-homa-modbus.conf.sample`. Для начала работы с
Modbus-устройствами необходимо создать корректный файл конфигурации и
запустить демона:

```
# cp /etc/wb-homa-modbus.conf.sample /etc/wb-homa-modbus.conf
# sensible-editor /etc/wb-homa-modbus.conf
# service wb-homa-modbus start
```

Возможен запуск демона вручную, что может быть полезно
для работы в отладочном режиме:

```
# service wb-homa-modbus stop
# wb-homa-modbus -c /etc/wb-homa-modbus.conf -d
```

Конфигурационный файл построен по трёхуровневой схеме:
порты (ports) -> устройства (devices) -> каналы (channels).
Конфигурация устройства device может быть задана двумя способами:
вручную прописать все параметры или задать только четыре параметра:
```
{
    // отображаемое имя устройства. Публикуется как
    // .../meta/name в MQTT
    "name" : "somename",
    
    // уникальый идентификатор устройства в MQTT.
    // каждое элемент в devices должен иметь уникальный id
    // topic'и, относящиеся в MQTT к данному устройству,
    // имеют общий префикс /devices/<идентификатор топика>/...
    "id" : "deviceID",
    
    // идентификатор Modbus slave
    "slave_id" : slaveID,
    
    "device_type" : "DeviceType"
    // по DeviceType драйвер будет искать  параметры в папке 
    // /usr/share/wb-homa-modbus/templates
}
```
Структура папки *templates* такова, что в каждом файле 
приведены параметры для одного типа устройств.
Также можно совместить первый способ со вторым, к вышеприведенным 
4 параметрам дописать конфигурацию для каналов, которые не прописаны
в соответствующем файле в папке templates.

Ниже приведён пример конфигурационного файла /etc/wb-homa-modbus.conf

```
{
    // опция debug включает или выключает отладочную печать.
    // Опция -d командной строки wb-homa-modbus также
    // включает отладочную печать и имеет приоритет над
    // данной опцией.
    "debug": false,

    // список портов
    "ports": [
        {
            // устройство, соответствующее порту RS-485
            "path" : "/dev/ttyNSC0",

            // скорость порта
            "baud_rate": 9600,

            // паритет - N, O или E (по умолчанию - N)
            "parity": "N",

            // количество бит данных (по умолчанию - 8)
            "data_bits": 8,

            // количество стоп-бит
            "stop_bits": 2,

            // интервал опроса устройств на порту в миллисекундах
            "poll_interval": 10,

            // включить/выключить порт. В случае задания
            // "enabled": false опрос порта и запись значений
            // каналов в устройства на данном порту не происходит.
            // По умолчанию - true.
            "enabled": true,

            // список устройств на данном порту
            "devices" : [
                {
                    // отображаемое имя устройства. Публикуется как
                    // .../meta/name в MQTT
                    "name": "MSU34+TLP",

                    // уникальый идентификатор устройства в MQTT.
                    // каждое элемент в devices должен иметь уникальный id
                    // topic'и, относящиеся в MQTT к данному устройству,
                    // имеют общий префикс /devices/<идентификатор топика>/...
                    "id": "msu34tlp",

                    // идентификатор Modbus slave
                    "slave_id": 2,

                    // включить/выключить устройство. В случае задания
                    // "enabled": false опрос устройства и запись значений
                    // его каналов не происходит. По умолчанию - true.
                    "enabled": true,

                    // список каналов устройства
                    "channels": [
                        {
                            // имя канала. topic'и, соответствующие каналу,
                            // публикуются как
                            // /devices/<идентификатор канала>/controls/<имя канала>
                            "name" : "Temp 1",

                            // тип регистра Modbus.
                            // возможные значения:
                            // "coil" - 1 бит, чтение/запись
                            // "discrete" - 1 бит, только чтение
                            // "holding" - 16 бит, чтение/запись
                            // "input" - 16 бит, только чтение
                            "reg_type" : "input",

                            // адрес регистра Modbus
                            "address" : 0,

                            // тип элемента управления в homA, например,
                            // "temperature", "text", "switch"
                            // Тип wo-switch задаёт вариант switch,
                            // для которого не производится опрос регистра -
                            // для таких каналов возможна только запись.
                            "type": "temperature",

                            // формат канала. Задаётся для регистров типа
                            // "holding" и "input". Возможные значения:
                            // "u16" - беззнаковое 16-битное целое
                            //         (используется по умолчанию)
                            // "s16" - знаковое 16-битное целое
                            // "u8" - беззнаковое 8-битное целое
                            // "s8" - знаковое 8-битное целое
                            "format": "s8"

                            // для регистров типа coil и discrete
                            // с типом отображения switch/wo-swich
                            // также допускается задание on_value -
                            // числового значения, соответствующего
                            // состоянию "on" (см. ниже)
                        },
                        {
                            // Ещё один канал
                            "name" : "Illuminance",
                            "reg_type" : "input",
                            "address" : 1,
                            "type": "text"
                        },
                        {
                            "name" : "Pressure",
                            "reg_type" : "input",
                            "address" : 2,
                            "type": "text",
                            "scale": 0.075
                        },
                        {
                            "name" : "Temp 2",
                            "reg_type" : "input",
                            "address" : 3,
                            "type": "temperature",
                            "format": "s8"
                        }
                    ]
                },
                {
                    // ещё одно устройство на канале
                    "name": "DRB88",
                    "id": "drb88",
                    "enabled": true,
                    "slave_id": 22,

                    // секция инициализации
                    "setup": [
                        {
                            // название регистра (для отладки)
                            // Выводится в случае включённой отладочной печати.
                            "title": "Input 0 type",
                            // адрес holding-регистра
                            "address": 1,
                            // значение для записи
                            "value": 1
                        },
                        {
                            "title": "Input 0 module",
                            "address": 3,
                            "value": 3 // was: 11
                        }
                    ],
                    "channels": [
                        {
                            "name" : "Relay 1",
                            "reg_type" : "coil",
                            "address" : 0,
                            "type": "switch"
                        },
                        {
                            "name" : "Relay 2",
                            "reg_type" : "coil",
                            "address" : 1,
                            "type": "switch"
                        },
                        // ...
                        {
                            "name" : "Input 2",
                            "reg_type" : "input",
                            "address" : 1,
                            "type": "switch",
                            // значение, соответствующее состоянию "on"
                            "on_value": 101
                        },
                        {
                            "name" : "Input 3",
                            "reg_type" : "input",
                            "address" : 2,
                            "type": "switch",
                            "on_value": 101
                        },
                        // ...
                    ]
                }
            ]
        },
        {
            // ещё один порт со своим набором устройств
            "path" : "/dev/ttyNSC1",
            "baud_rate": 9600,
            "parity": "N",
            "data_bits": 8,
            "stop_bits": 1,
            "poll_interval": 100,
            "enabled": true,
            "devices" : [
                {
                    "name": "tM-P3R3",
                    "id": "tmp3r3",
                    "enabled": true,
                    "slave_id": 1,
                    "channels": [
                        {
                            "name" : "Relay 0",
                            "reg_type" : "coil",
                            "address" : 0,
                            "type": "switch"
                        },
                        // ...
                    ]
                },
                // ...
            ]
        }
    ]
}
``

Устройства Uniel
-------------
В драйвере wb-homa-modbus реализована поддержка некоторых устройств Uniel (smart.uniel.ru).
Эти устройства используют собственный протокол, отличный от Modbus-RTU.

Поддерживаются устройства:
UCH-M111RX/0808 (модуль реле), UCH-M131RC/0808 (фазовый диммер на 220В), UCH-M141RC/0808 (диммер светодиодных лент),
UCH-M121RX/0808 (модуль реле с встроенной логикой, работа не проверялась).


Для работы устройств необходимо выставить тип "uniel" для порта, работа с устройствами Modbus на одном порте не поддерживается.

Все типы регистров (input, holding, coil) равнозначны и соответствуют однобайтным регистрам ("параметрам") протокола Uniel,
чтение и запись которых производится через команды 0x05 и 0x06. Допустимые значения адреса регистров: 0-255 ("0x00"-"0xff").

Дополнительно поддерживаются регистры, доступные на запись через команду 0x0A, а на чтение через команду 0x05.
Такие регистры обозначаются следующим образом:
```
"address": "0x0100WWRR"
```
здесь WW - шестнадцатиречный адрес регистра (параметра) для записи с помощью команды 0x0A,
RR - шестнадцатиречный адрес регистра (параметра) для чтения командой 0x05.

Пример (чтение по адресу 0x41, запись командой 0x0A по адресу 0x01):

```
                        {
                            "name" : "LED 1",
                            "reg_type": "holding",
                            "address": "0x01000141",
                            "type": "range",
                            "max": "0xff"
                        },
```
Для устройств типа "uniel" также существуют шаблоны конфигурации, для примера в приведенном ниже файле, 
первое устройство задано через шабло, второй device черз шаблон, но параметры "name" и "id" заданы, и также можно добавить конфиг для канала, который
добавится к тем, что есть в шаблоне, и последнее устройство записано вручную. Шаблоны для устройств типа "uniel" также находятся
в папке /usr/share/wb-homa-modbus/templates/

См. также: [пример конфигурационного файла](wb-homa-modbus/config-uniel.json).
