import { useState, useRef, useEffect } from 'react'
import { BASE_URL } from '../api/axios';
import useAxiosPrivate from '../hooks/useAxiosPrivate';

interface Props {
    id: number;
    chapterId: number;
    alt: string;
    setIsWide?: React.Dispatch<React.SetStateAction<boolean>>;
    onClick?: React.MouseEventHandler<HTMLImageElement>
}

export default function Image({ chapterId, id, alt, setIsWide, onClick }: Props) {
    const axiosPrivate = useAxiosPrivate();
    const imageRef = useRef<HTMLImageElement>(null);
    const [done, setDone] = useState(false);

    useEffect(() => {
        let objectUrl: string
        const ev = () => {
            if (setIsWide) {
                if (imageRef.current!!.naturalWidth > imageRef.current!!.naturalHeight)
                    setIsWide(true);
                else
                    setIsWide(false);
            }
            URL.revokeObjectURL(objectUrl);
        }

        const imageOnLoad = async () => {
            // Fetch the image.
            try {
                const response = await axiosPrivate.get(`${BASE_URL}/api/chapter/${chapterId}/page/${id}`, { responseType: 'blob' });

                // Create an object URL from the data.
                const blob = response.data;
                objectUrl = URL.createObjectURL(blob);

                // Update the source of the image.
                imageRef.current!!.src = objectUrl;

            } catch (err: unknown) {
                console.log(err);
            }
            imageRef.current?.addEventListener('load', ev);
            setDone(true);
        }
        imageOnLoad();

        return () => {
            imageRef.current?.removeEventListener('load', ev);
            setDone(false);
        }
    }, [id]);
    console.log(`${BASE_URL}/api/chapter/${chapterId}/page/${id}`);

    return (
        <img style={{ height: '80vh' }} ref={imageRef} alt={alt} onClick={onClick} />
    )
}
