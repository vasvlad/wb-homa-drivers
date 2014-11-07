package smartbus

import (
	"io"
	"encoding/binary"
	"log"
)

func ReadChannelStatus(reader io.Reader) ([]bool, error) {
	var n uint8
	if err := binary.Read(reader, binary.BigEndian, &n); err != nil {
		log.Printf("error reading channel status header: %v", err)
		return nil, err
	}
	status := make([]bool, n)
	if n > 0 {
		bs := make([]uint8, (n + 7) / 8)
		if _, err := io.ReadFull(reader, bs); err != nil {
			log.Printf("error reading channel status: %v", err)
			return nil, err
		}
		for i := range status {
			status[i] = (bs[i / 8] >> uint(i % 8)) & 1 == 1
		}
	}

	return status, nil
}

func WriteChannelStatus(writer io.Writer, status []bool) {
	binary.Write(writer, binary.BigEndian, uint8(len(status)))
	if len(status) == 0 {
		return
	}
	bs := make([]uint8, (len(status) + 7) / 8)
	for i, v := range status {
		if v {
			bs[i / 8] |= 1 << uint(i % 8)
		}
	}
	binary.Write(writer, binary.BigEndian, bs)
}