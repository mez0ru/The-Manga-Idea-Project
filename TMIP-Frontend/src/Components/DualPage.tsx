import { useState, useRef, useEffect } from 'react'
import Grid from '@mui/material/Grid';
import { BASE_URL } from '../api/axios';
import { useNavigate, useSearchParams } from 'react-router-dom';

interface Props {
    array: Array<number>;
    chapterId: number;
    alt?: string;
    onClick?: React.MouseEventHandler<HTMLImageElement>;
    currentId: number;
    setCurrentId: React.Dispatch<React.SetStateAction<number>>;
    // onClick: React.MouseEventHandler<HTMLImageElement>;
}

export default function DualPage({ chapterId, array, alt = "whatever", onClick, currentId, setCurrentId }: Props) {
    const [searchParams] = useSearchParams();
    const navigate = useNavigate()
    const prevCommand = useRef(false); // if previous is triggered

    const [isWide1, setIsWide1] = useState(false);
    const [isWide2, setIsWide2] = useState(false);

    const refImg1 = useRef<HTMLImageElement>(null);
    const refImg2 = useRef<HTMLImageElement>(null);



    const forward = () => {
        if (currentId !== array.length - 2) {
            setCurrentId(x => {
                refImg1.current!.hidden = true;
                prevCommand.current = false;
                if (isWide1 || isWide2 || x === 0) {

                    // searchParams.set("page", `${x + 1}`);
                    return x + 1;
                }
                else {
                    refImg2.current!.hidden = true;
                    // searchParams.set("page", `${x + 2}`);
                    return x + 2;
                }
            })
        }
    }

    const backward = () => {
        if (currentId !== 0) {
            setCurrentId(x => {
                refImg1.current!.hidden = true;
                if (isWide2) {
                    // searchParams.set("page", `${x - 1}`);
                    return x - 1;
                }
                else {
                    if (refImg2.current)
                        refImg2.current.hidden = true;
                    // searchParams.set("page", `${x - 2}`);
                    prevCommand.current = true;
                    return x - 2;
                }
            })
        }
    }


    const onImgClick = (e: React.MouseEvent<HTMLImageElement>) => {
        if (e.clientY > e.currentTarget.clientHeight / 1.5) {
            // forward
            forward();
        } else if (e.clientY < e.currentTarget.clientHeight / 3) {
            // previous
            backward();
        } else {
            if (onClick)
                onClick(e);
        }
    }

    useEffect(() => {
        const params = new URLSearchParams()
        if (currentId) {
            params.append("page", currentId.toString());
        } else {
            params.delete("page")
        }

        navigate({ search: params.toString() })

        const keyDownEvent = (e: KeyboardEvent) => {
            if (e.key === 'ArrowDown') {
                forward();
            } else if (e.key === 'ArrowUp') {
                backward();
            }
        }

        document.addEventListener('keydown', keyDownEvent);

        return () => {
            document.removeEventListener('keydown', keyDownEvent);
        }
    }, [currentId, isWide1, isWide2, navigate])

    const onLoad = (e: React.UIEvent<HTMLImageElement>) => {
        if (e.currentTarget.naturalWidth > e.currentTarget.naturalHeight) {
            if (prevCommand.current) {
                prevCommand.current = false;
                setCurrentId(x => x + 1);
                return;
            }
            if (e.currentTarget.alt === '1') {
                e.currentTarget.removeAttribute("hidden");
                e.currentTarget.style.removeProperty("float");
                e.currentTarget.style.display = 'block';
                e.currentTarget.style.margin = '0 auto';
                setIsWide1(true);
            } else {
                if (refImg1.current?.complete && refImg1.current?.naturalWidth < refImg1.current?.naturalHeight)
                    refImg1.current?.removeAttribute("hidden");
                e.currentTarget.setAttribute("hidden", "true");
                setIsWide2(true);
            }

        } else {

            if (e.currentTarget.alt === '1') {
                if (currentId !== 0 && currentId !== array.length - 1) {
                    e.currentTarget.style.float = 'right';

                    if (refImg2.current?.complete) {
                        if (refImg2.current?.naturalWidth < refImg2.current?.naturalHeight)
                            refImg2.current?.removeAttribute("hidden");
                        e.currentTarget.removeAttribute("hidden");
                    }
                } else {
                    if (prevCommand.current) {
                        prevCommand.current = false;
                        setCurrentId(x => x + 1);
                        return;
                    }
                    e.currentTarget.removeAttribute("hidden");
                }

                e.currentTarget.style.removeProperty("display");
                e.currentTarget.style.removeProperty("margin");
                setIsWide1(false);

            } else {
                setIsWide2(false);
                if (refImg1.current?.complete && refImg1.current?.naturalWidth < refImg1.current?.naturalHeight) {
                    refImg1.current?.removeAttribute("hidden");
                    e.currentTarget.removeAttribute("hidden");
                }
            }
        }
    }

    if (currentId !== 0 && currentId !== array.length - 1)
        return (
            <Grid container>
                <Grid item xs={isWide1 ? 12 : 6}>
                    <img style={{ height: '100vh', float: 'right' }} ref={refImg1} alt='1' src={`${BASE_URL}/api/chapter/${chapterId}/page/${array[currentId]}`} onClick={onImgClick} onLoad={onLoad} hidden />
                </Grid>
                <Grid item xs={6} hidden={isWide1}>
                    <img style={{ height: '100vh', float: 'left' }} ref={refImg2} alt='2' src={`${BASE_URL}/api/chapter/${chapterId}/page/${array[currentId + 1]}`} onClick={onImgClick} onLoad={onLoad} hidden />
                </Grid>
            </Grid>);
    else

        return (
            <Grid container>
                <Grid item xs={6}>
                </Grid>
                <Grid item xs={6}>
                    <img style={{ height: '100vh', float: 'left' }} ref={refImg1} alt='1' onClick={onImgClick} onLoad={onLoad} src={`${BASE_URL}/api/chapter/${chapterId}/page/${array[currentId]}`} hidden />
                </Grid>
            </Grid>
        )
}
